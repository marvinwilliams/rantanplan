#include "grounder/grounder.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "model/to_string.hpp"
#include "planner/planner.hpp"
#include "util/timer.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace normalized;
using namespace std::chrono_literals;

Grounder::Grounder(const std::shared_ptr<Problem> &problem) noexcept
    : problem_{problem} {
  num_actions_ =
      std::accumulate(problem_->actions.begin(), problem_->actions.end(), 0ul,
                      [this](uint_fast64_t sum, const auto &a) {
                        return sum + get_num_instantiated(a, *problem_);
                      });

  trivially_rigid_.resize(problem_->predicates.size(), true);
  trivially_useless_.resize(problem_->predicates.size(), true);
  for (const auto &action : problem_->actions) {
    for (const auto &precondition : action.preconditions) {
      trivially_useless_[precondition.atom.predicate] = false;
    }
    for (const auto &[precondition, positive] : action.ground_preconditions) {
      trivially_useless_[precondition.predicate] = false;
    }
    for (const auto &effect : action.effects) {
      trivially_rigid_[effect.atom.predicate] = false;
    }
    for (const auto &[effect, positive] : action.ground_effects) {
      trivially_useless_[effect.predicate] = false;
    }
  }

  init_.resize(problem_->predicates.size());
  for (const auto &init : problem_->init) {
    init_[init.predicate].push_back(get_id(init));
  }
  for (auto &i : init_) {
    std::sort(i.begin(), i.end());
  }

  goal_.resize(problem_->predicates.size());
  for (const auto &[goal, positive] : problem_->goal) {
    goal_[goal.predicate].push_back(get_id(goal));
  }
  for (auto &g : goal_) {
    std::sort(g.begin(), g.end());
  }

  actions_.reserve(problem_->actions.size());

  for (const auto &action : problem_->actions) {
    actions_.push_back({action});
  }

  successful_cache_.resize(problem_->predicates.size());
  unsuccessful_cache_.resize(problem_->predicates.size());

  prune_actions();

  groundness_ = static_cast<float>(get_num_actions() + num_pruned_actions_) /
                static_cast<float>(num_actions_);

  parameter_selector_ = std::invoke([]() {
    switch (config.parameter_selection) {
    case Config::ParameterSelection::MostFrequent:
      return &Grounder::select_most_frequent;
    case Config::ParameterSelection::MinNew:
      return &Grounder::select_min_new;
    case Config::ParameterSelection::MaxRigid:
      return &Grounder::select_max_rigid;
    case Config::ParameterSelection::ApproxMinNew:
      return &Grounder::select_approx_min_new;
    case Config::ParameterSelection::ApproxMaxRigid:
      return &Grounder::select_approx_max_rigid;
    case Config::ParameterSelection::FirstEffect:
      return &Grounder::select_first_effect;
    }
    return &Grounder::select_approx_max_rigid;
  });
}

bool Grounder::is_rigid(const GroundAtom &atom, bool positive) const noexcept {
  switch (config.cache_policy) {
  case Config::CachePolicy::None:
    return is_rigid<false, false>(atom, positive);
  case Config::CachePolicy::NoUnsuccessful:
    return is_rigid<true, false>(atom, positive);
  case Config::CachePolicy::Unsuccessful:
    return is_rigid<true, true>(atom, positive);
  default:
    assert(false);
    return is_rigid<true, true>(atom, positive);
  }
}

bool Grounder::is_useless(const GroundAtom &atom) const noexcept {
  switch (config.cache_policy) {
  case Config::CachePolicy::None:
    return is_useless<false, false>(atom);
  case Config::CachePolicy::NoUnsuccessful:
    return is_useless<true, false>(atom);
  case Config::CachePolicy::Unsuccessful:
    return is_useless<true, true>(atom);
  default:
    assert(false);
    return is_useless<true, true>(atom);
  }
}

void Grounder::refine(float groundness, util::Seconds timeout) {
  util::Timer timer;

  while (groundness_ < groundness) {
    if (timer.get_elapsed_time() > timeout) {
      return;
    }
    if (global_timer.get_elapsed_time() > config.timeout) {
      throw TimeoutException{};
    }
    bool is_grounding = false;
    for (auto &action_list : actions_) {
      std::vector<Action> new_actions;
      uint_fast64_t new_pruned_actions = 0;
      for (const auto &action : action_list) {
        auto selection = std::invoke(parameter_selector_, *this, action);
        if (!selection.empty()) {
          is_grounding = true;
        }
        for (auto it = AssignmentIterator{selection, action, *problem_};
             it != AssignmentIterator{}; ++it) {
          auto [new_action, valid] = ground(action, *it);
          if (valid) {
            new_actions.push_back(new_action);
          } else {
            new_pruned_actions += get_num_instantiated(new_action, *problem_);
          }
        }
      }
      num_pruned_actions_ += new_pruned_actions;
      action_list = std::move(new_actions);
      groundness_ =
          static_cast<float>(get_num_actions() + num_pruned_actions_) /
          static_cast<float>(num_actions_);
      if (groundness_ >= groundness) {
        break;
      }
    }
    if (!is_grounding) {
      return;
    }
    prune_actions();
  }
}

size_t Grounder::get_num_actions() const noexcept {
  uint_fast64_t sum = 0;
  for (const auto &actions : actions_) {
    sum += actions.size();
  }
  return sum;
}

float Grounder::get_groundness() const noexcept { return groundness_; }

Grounder::PredicateId Grounder::get_id(const GroundAtom &atom) const noexcept {
  uint_fast64_t result = 0;
  for (const auto &a : atom.arguments) {
    result = (result * problem_->constants.size()) + a;
  }
  return result;
}

bool Grounder::is_trivially_rigid(const GroundAtom &atom, bool positive) const
    noexcept {
  if (std::binary_search(init_[atom.predicate].begin(),
                         init_[atom.predicate].end(),
                         get_id(atom)) != positive) {
    return false;
  }
  return trivially_rigid_[atom.predicate];
}

bool Grounder::is_trivially_useless(const GroundAtom &atom) const noexcept {
  auto id = get_id(atom);
  if (std::binary_search(goal_[atom.predicate].begin(),
                         goal_[atom.predicate].end(), id)) {
    return false;
  }
  return trivially_useless_[atom.predicate];
}

bool Grounder::has_precondition(const Action &action,
                                const GroundAtom &atom) const noexcept {
  for (const auto &[precondition, _] : action.ground_preconditions) {
    if (precondition == atom) {
      return true;
    }
  }
  for (const auto &precondition : action.preconditions) {
    if (precondition.atom.predicate == atom.predicate) {
      if (is_instantiatable(precondition.atom, atom.arguments, action,
                            *problem_)) {
        return true;
      }
    }
  }
  return false;
}

bool Grounder::has_effect(const Action &action, const GroundAtom &atom,
                          bool positive) const noexcept {
  for (const auto &[effect, effect_positive] : action.ground_effects) {
    if (effect == atom && effect_positive == positive) {
      return true;
    }
  }
  for (const auto &effect : action.effects) {
    if (effect.atom.predicate == atom.predicate &&
        effect.positive == positive) {
      if (is_instantiatable(effect.atom, atom.arguments, action, *problem_)) {
        return true;
      }
    }
  }
  return false;
}

ParameterSelection Grounder::select_most_frequent(const Action &action) const
    noexcept {
  std::vector<unsigned int> frequency(action.parameters.size(), 0);

  for (const auto &condition : action.preconditions) {
    for (const auto &a : condition.atom.arguments) {
      if (a.is_parameter()) {
        ++frequency[a.get_parameter_index()];
      }
    }
  }
  for (const auto &condition : action.effects) {
    for (const auto &a : condition.atom.arguments) {
      if (a.is_parameter()) {
        ++frequency[a.get_parameter_index()];
      }
    }
  }
  size_t max_index = 0;
  for (size_t i = 1; i < action.parameters.size(); ++i) {
    if (frequency[i] > frequency[max_index]) {
      max_index = i;
    }
  }
  if (action.parameters[max_index].is_free()) {
    return {max_index};
  }
  return {};
}

ParameterSelection Grounder::select_min_new(const Action &action) const
    noexcept {
  auto min_it = action.preconditions.end();
  uint_fast64_t min = std::numeric_limits<uint_fast64_t>::max();

  for (auto it = action.preconditions.begin(); it != action.preconditions.end();
       ++it) {
    uint_fast64_t current = get_num_instantiated(
        get_referenced_parameters(it->atom, action), action, *problem_);
    for (auto ground_atom_it = GroundAtomIterator{it->atom, action, *problem_};
         ground_atom_it != GroundAtomIterator{}; ++ground_atom_it) {
      if (is_rigid(*ground_atom_it, !it->positive)) {
        --current;
      }
    }
    if (current < min) {
      min = current;
      min_it = it;
    }
  }
  if (min_it == action.preconditions.end()) {
    return select_most_frequent(action);
  }
  return get_referenced_parameters(min_it->atom, action);
}

ParameterSelection Grounder::select_max_rigid(const Action &action) const
    noexcept {
  auto max_it = action.preconditions.end();
  uint_fast64_t max = 0;

  for (auto it = action.preconditions.begin(); it != action.preconditions.end();
       ++it) {
    if (1 + get_num_instantiated(get_referenced_parameters(it->atom, action),
                                 action, *problem_) <=
        max) {
      continue;
    }
    uint_fast64_t current = 1;
    for (auto ground_atom_it = GroundAtomIterator{it->atom, action, *problem_};
         ground_atom_it != GroundAtomIterator{}; ++ground_atom_it) {
      if (is_rigid(*ground_atom_it, !it->positive)) {
        ++current;
      }
    }
    if (current > max) {
      max = current;
      max_it = it;
    }
  }
  if (max_it == action.preconditions.end()) {
    return select_most_frequent(action);
  }
  return get_referenced_parameters(max_it->atom, action);
}

ParameterSelection Grounder::select_approx_min_new(const Action &action) const
    noexcept {
  auto min_it = action.preconditions.end();
  uint_fast64_t min = std::numeric_limits<uint_fast64_t>::max();

  for (auto it = action.preconditions.begin(); it != action.preconditions.end();
       ++it) {
    uint_fast64_t current = get_num_instantiated(
        get_referenced_parameters(it->atom, action), action, *problem_);
    if (current < min) {
      min = current;
      min_it = it;
    }
  }
  if (min_it == action.preconditions.end()) {
    return select_most_frequent(action);
  }
  return get_referenced_parameters(min_it->atom, action);
}

ParameterSelection Grounder::select_approx_max_rigid(const Action &action) const
    noexcept {
  auto max_it = action.preconditions.end();
  uint_fast64_t max = 0;

  for (auto it = action.preconditions.begin(); it != action.preconditions.end();
       ++it) {
    uint_fast64_t current =
        1 + (it->positive
                 ? successful_cache_[it->atom.predicate].neg_rigid.size()
                 : successful_cache_[it->atom.predicate].pos_rigid.size());

    if (current > max) {
      max = current;
      max_it = it;
    }
  }
  if (max_it == action.preconditions.end()) {
    return select_most_frequent(action);
  }
  return get_referenced_parameters(max_it->atom, action);
}

ParameterSelection Grounder::select_first_effect(const Action &action) const
    noexcept {
  if (action.effects.empty()) {
    return select_most_frequent(action);
  } else {
    return get_referenced_parameters(action.effects[0].atom, action);
  }
}

void Grounder::prune_actions() noexcept {
  bool changed;
  do {
    changed = false;
    if (config.cache_policy == Config::CachePolicy::Unsuccessful) {
      for (auto &c : unsuccessful_cache_) {
        c.pos_rigid.clear();
        c.neg_rigid.clear();
        c.useless.clear();
      }
    }
    for (size_t i = 0; i < actions_.size(); ++i) {
      auto it = std::partition(actions_[i].begin(), actions_[i].end(),
                               [this](const auto &a) { return is_valid(a); });
      if (it != actions_[i].end()) {
        std::for_each(it, actions_[i].end(), [&](const auto &a) {
          num_pruned_actions_ += get_num_instantiated(a, *problem_);
        });
        actions_[i].erase(it, actions_[i].end());
        changed = true;
      }
      for (auto &a : actions_[i]) {
        if (simplify(a)) {
          changed = true;
        }
      }
    }
  } while (changed);
}

bool Grounder::is_valid(const Action &action) const noexcept {
  if (action.ground_effects.empty() && action.effects.empty()) {
    return false;
  }
  if (std::any_of(action.ground_preconditions.begin(),
                  action.ground_preconditions.end(), [this](const auto &p) {
                    return is_rigid(p.first, !p.second);
                  })) {
    return false;
  }
  if (config.pruning_policy == Config::PruningPolicy::Eager &&
      std::any_of(action.preconditions.begin(), action.preconditions.end(),
                  [&](const auto &precondition) {
                    for (auto it = GroundAtomIterator{precondition.atom, action,
                                                      *problem_};
                         it != GroundAtomIterator{}; ++it) {
                      if (!is_rigid(*it, !precondition.positive)) {
                        return false;
                      }
                    }
                    return true;
                  })) {
    return false;
  }

  if (action.effects.empty() &&
      std::all_of(action.ground_effects.begin(), action.ground_effects.end(),
                  [this](const auto &e) {
                    return is_rigid(e.first, e.second) || is_useless(e.first);
                  })) {
    return false;
  }
  return true;
}

std::pair<normalized::Action, bool>
Grounder::ground(const normalized::Action &action,
                 const normalized::ParameterAssignment &assignment) const
    noexcept {
  Action new_action{};
  new_action.id = action.id;
  new_action.parameters = action.parameters;

  for (auto [p, c] : assignment) {
    new_action.parameters[p].set(c);
  }
  for (const auto &[precondition, positive] : action.ground_preconditions) {
    if (is_rigid(precondition, !positive)) {
      return {new_action, false};
    } else if (!is_rigid(precondition, positive)) {
      new_action.ground_preconditions.emplace_back(precondition, positive);
    }
  }
  for (auto precondition : action.preconditions) {
    if (update_condition(precondition, new_action)) {
      auto new_precondition = as_ground_atom(precondition.atom);
      if (is_rigid(new_precondition, !precondition.positive)) {
        return {new_action, false};
      } else if (!is_rigid(new_precondition, precondition.positive)) {
        new_action.ground_preconditions.emplace_back(new_precondition,
                                                     precondition.positive);
      }
    } else {
      if (config.pruning_policy == Config::PruningPolicy::Eager) {
        bool unsatisfiable = true;
        for (auto it = GroundAtomIterator{precondition.atom, action, *problem_};
             it != GroundAtomIterator{}; ++it) {
          if (!is_rigid(*it, !precondition.positive)) {
            unsatisfiable = false;
            break;
          }
        }
        if (unsatisfiable) {
          return {new_action, false};
        }
        new_action.preconditions.push_back(std::move(precondition));
      } else {
        new_action.preconditions.push_back(std::move(precondition));
      }
    }
  }
  for (const auto &[effect, positive] : action.ground_effects) {
    if (!is_rigid(effect, positive) && !is_useless(effect)) {
      new_action.ground_effects.emplace_back(effect, positive);
    }
  }
  for (auto effect : action.effects) {
    if (update_condition(effect, new_action)) {
      auto new_effect = as_ground_atom(effect.atom);
      if (!is_rigid(new_effect, effect.positive) && !is_useless(new_effect)) {
        new_action.ground_effects.emplace_back(new_effect, effect.positive);
      }
    } else {
      if (config.pruning_policy == Config::PruningPolicy::Eager) {
        bool keep_effect = false;
        for (auto it = GroundAtomIterator{effect.atom, action, *problem_};
             it != GroundAtomIterator{}; ++it) {
          if (!is_rigid(*it, effect.positive) && !is_useless(*it)) {
            keep_effect = true;
            break;
          }
        }
        if (keep_effect) {
          new_action.effects.push_back(std::move(effect));
        }
      } else {
        new_action.effects.push_back(std::move(effect));
      }
    }
  }

  if (new_action.ground_effects.empty() && new_action.effects.empty()) {
    return {new_action, false};
  }
  return {new_action, true};
}

bool Grounder::simplify(Action &action) const noexcept {
  bool changed = false;
  if (auto it = std::remove_if(
          action.ground_effects.begin(), action.ground_effects.end(),
          [this](const auto &e) {
            return is_rigid(e.first, e.second) || is_useless(e.first);
          });
      it != action.ground_effects.end()) {
    action.ground_effects.erase(it, action.ground_effects.end());
    changed = true;
  }

  if (auto it = std::remove_if(
          action.ground_preconditions.begin(),
          action.ground_preconditions.end(),
          [this](const auto &p) { return is_rigid(p.first, p.second); });
      it != action.ground_preconditions.end()) {
    action.ground_preconditions.erase(it, action.ground_preconditions.end());
    changed = true;
  }
  return changed;
}

std::shared_ptr<Problem> Grounder::extract_problem() const noexcept {
  auto preprocessed_problem = std::make_shared<Problem>();
  preprocessed_problem->domain_name = problem_->domain_name;
  preprocessed_problem->problem_name = problem_->problem_name;
  preprocessed_problem->requirements = problem_->requirements;
  preprocessed_problem->types = problem_->types;
  preprocessed_problem->type_names = problem_->type_names;
  preprocessed_problem->constants = problem_->constants;
  preprocessed_problem->constant_names = problem_->constant_names;
  preprocessed_problem->constants_of_type = problem_->constants_of_type;
  preprocessed_problem->constant_type_map = problem_->constant_type_map;
  preprocessed_problem->predicates = problem_->predicates;
  preprocessed_problem->predicate_names = problem_->predicate_names;
  preprocessed_problem->init = problem_->init;
  std::copy_if(problem_->goal.begin(), problem_->goal.end(),
               std::back_inserter(preprocessed_problem->goal),
               [this](const auto &g) { return !is_rigid(g.first, g.second); });
  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    for (auto &action : actions_[i]) {
      preprocessed_problem->actions.push_back(std::move(action));
    }
  }
  preprocessed_problem->action_names = problem_->action_names;

  return preprocessed_problem;
}
