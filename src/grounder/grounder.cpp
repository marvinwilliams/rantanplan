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
    for (const auto &effect : action.effects) {
      trivially_rigid_[effect.atom.predicate] = false;
    }
  }

  init_.resize(problem_->predicates.size());
  for (const auto &init : problem_->init) {
    init_[init.predicate].push_back(get_id(init));
  }
  std::for_each(init_.begin(), init_.end(),
                [](auto &i) { std::sort(i.begin(), i.end()); });

  pos_goal_.resize(problem_->predicates.size());
  neg_goal_.resize(problem_->predicates.size());
  for (const auto &[goal, positive] : problem_->goal) {
    if (positive) {
      pos_goal_[goal.predicate].push_back(get_id(goal));
    } else {
      neg_goal_[goal.predicate].push_back(get_id(goal));
    }
  }
  std::for_each(pos_goal_.begin(), pos_goal_.end(),
                [](auto &g) { std::sort(g.begin(), g.end()); });
  std::for_each(neg_goal_.begin(), neg_goal_.end(),
                [](auto &g) { std::sort(g.begin(), g.end()); });

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
        std::for_each(AssignmentIterator{selection, action, *problem_},
                      AssignmentIterator{},
                      [&](const ParameterAssignment &assignment) {
                        Action new_action = action;
                        ground(assignment, new_action);
                        if (is_valid(new_action)) {
                          simplify(new_action);
                          new_actions.push_back(std::move(new_action));
                        } else {
                          new_pruned_actions +=
                              get_num_instantiated(new_action, *problem_);
                        }
                      });
      };
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
    result = (result * problem_->constants.size()) + a.id;
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
  if (std::binary_search(pos_goal_[atom.predicate].begin(),
                         pos_goal_[atom.predicate].end(), id) ||
      std::binary_search(neg_goal_[atom.predicate].begin(),
                         neg_goal_[atom.predicate].end(), id)) {
    return false;
  }
  return trivially_useless_[atom.predicate];
}

bool Grounder::has_effect(const Action &action, const GroundAtom &atom,
                          bool positive) const noexcept {
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

bool Grounder::has_precondition(const Action &action, const GroundAtom &atom,
                                bool positive) const noexcept {
  for (const auto &precondition : action.preconditions) {
    if (precondition.atom.predicate == atom.predicate &&
        precondition.positive == positive) {
      if (is_instantiatable(precondition.atom, atom.arguments, action,
                            *problem_)) {
        return true;
      }
    }
  }
  return false;
}

bool Grounder::has_precondition(const Action &action,
                                const GroundAtom &atom) const noexcept {
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

// No action has this predicate as effect and it is not in init_
bool Grounder::is_rigid(const GroundAtom &atom, bool positive) const noexcept {
  auto &rigid = (positive ? successful_cache_[atom.predicate].pos_rigid
                          : successful_cache_[atom.predicate].neg_rigid);
  auto &not_rigid = (positive ? unsuccessful_cache_[atom.predicate].pos_rigid
                              : unsuccessful_cache_[atom.predicate].neg_rigid);
  auto id = get_id(atom);

  if ((config.cache_policy != Config::CachePolicy::None) &&
      (rigid.find(id) != rigid.end())) {
    return true;
  }

  if ((config.cache_policy == Config::CachePolicy::Unsuccessful) &&
      (not_rigid.find(id) != not_rigid.end())) {
    return false;
  }

  if (std::binary_search(init_[atom.predicate].begin(),
                         init_[atom.predicate].end(), id) != positive) {
    if (config.cache_policy == Config::CachePolicy::Unsuccessful) {
      not_rigid.insert(id);
    }
    return false;
  }

  if (trivially_rigid_[atom.predicate]) {
    if (config.cache_policy != Config::CachePolicy::None) {
      rigid.insert(id);
    }
    return true;
  }

  if (config.pruning_policy == Config::PruningPolicy::Trivial) {
    if (config.cache_policy == Config::CachePolicy::Unsuccessful) {
      not_rigid.insert(id);
    }
    return false;
  }

  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    const auto &base_action = problem_->actions[i];
    if (!has_effect(base_action, atom, !positive)) {
      continue;
    }
    for (const auto &action : actions_[i]) {
      if (has_effect(action, atom, !positive)) {
        if (config.cache_policy == Config::CachePolicy::Unsuccessful) {
          not_rigid.insert(id);
        }
        return false;
      }
    }
  }
  if (config.cache_policy != Config::CachePolicy::None) {
    rigid.insert(id);
  }
  return true;
}

// No action has this predicate as precondition and it is a not a goal
bool Grounder::is_useless(const GroundAtom &atom) const noexcept {
  auto id = get_id(atom);
  auto &useless = successful_cache_[atom.predicate].useless;
  auto &not_useless = unsuccessful_cache_[atom.predicate].useless;

  if ((config.cache_policy != Config::CachePolicy::None) &&
      (useless.find(id) != useless.end())) {
    return true;
  }

  if ((config.cache_policy == Config::CachePolicy::Unsuccessful) &&
      (not_useless.find(id) != not_useless.end())) {
    return false;
  }

  if (std::binary_search(pos_goal_[atom.predicate].begin(),
                         pos_goal_[atom.predicate].end(), id) ||
      std::binary_search(neg_goal_[atom.predicate].begin(),
                         neg_goal_[atom.predicate].end(), id)) {
    if (config.cache_policy == Config::CachePolicy::Unsuccessful) {
      not_useless.insert(id);
    }
    return false;
  }

  if (trivially_useless_[atom.predicate]) {
    if (config.cache_policy != Config::CachePolicy::None) {
      useless.insert(id);
    }
    return true;
  }

  if (config.pruning_policy == Config::PruningPolicy::Trivial) {
    if (config.cache_policy == Config::CachePolicy::Unsuccessful) {
      not_useless.insert(id);
    }
    return false;
  }

  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    const auto &action = problem_->actions[i];
    if (has_precondition(action, atom)) {
      for (const auto &action : actions_[i]) {
        if (has_precondition(action, atom)) {
          if (config.cache_policy == Config::CachePolicy::Unsuccessful) {
            not_useless.insert(id);
          }
          return false;
        }
      }
    }
  }

  if (config.cache_policy != Config::CachePolicy::None) {
    useless.insert(id);
  }
  return true;
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
    if (is_ground(it->atom)) {
      continue;
    }
    uint_fast64_t current = get_num_instantiated(
        get_referenced_parameters(it->atom, action), action, *problem_);
    std::for_each(GroundAtomIterator{it->atom, action, *problem_},
                  GroundAtomIterator{}, [&](const auto &p) {
                    if (is_rigid(p, !it->positive)) {
                      --current;
                    }
                  });
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
    if (is_ground(it->atom) ||
        get_num_instantiated(get_referenced_parameters(it->atom, action),
                             action, *problem_) +
                1 <=
            max) {
      continue;
    }
    uint_fast64_t current = 1;
    std::for_each(GroundAtomIterator{it->atom, action, *problem_},
                  GroundAtomIterator{}, [&](const auto &p) {
                    if (is_rigid(p, !it->positive)) {
                      ++current;
                    }
                  });
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
    if (is_ground(it->atom)) {
      continue;
    }
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
    if (is_ground(it->atom)) {
      continue;
    }
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
  for (const auto &effect : action.effects) {
    if (!is_ground(effect.atom)) {
      return get_referenced_parameters(effect.atom, action);
    }
  }
  return select_most_frequent(action);
}

void Grounder::prune_actions() noexcept {
  bool changed;
  do {
    changed = false;
    if (config.cache_policy == Config::CachePolicy::Unsuccessful) {
      std::for_each(unsuccessful_cache_.begin(), unsuccessful_cache_.end(),
                    [](auto &c) {
                      c.pos_rigid.clear();
                      c.neg_rigid.clear();
                      c.useless.clear();
                    });
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
      std::for_each(actions_[i].begin(), actions_[i].end(), [&](auto &a) {
        if (simplify(a)) {
          changed = true;
        }
      });
    }
  } while (changed);
}

bool Grounder::is_valid(const Action &action) const noexcept {
  for (const auto &precondition : action.preconditions) {
    if (config.pruning_policy != Config::PruningPolicy::Eager &&
        !is_ground(precondition.atom)) {
      continue;
    }
    if (std::all_of(GroundAtomIterator{precondition.atom, action, *problem_},
                    GroundAtomIterator{}, [&](const auto &p) {
                      return is_rigid(p, !precondition.positive);
                    })) {
      return false;
    }
  }

  if (config.pruning_policy != Config::PruningPolicy::Eager &&
      std::any_of(action.effects.begin(), action.effects.end(),
                  [&](const auto &e) { return !is_ground(e.atom); })) {
    return true;
  }
  if (std::all_of(
          action.effects.begin(), action.effects.end(), [&](const auto &e) {
            return std::all_of(GroundAtomIterator{e.atom, action, *problem_},
                               GroundAtomIterator{}, [&](const auto &p) {
                                 return is_rigid(p, e.positive) ||
                                        is_useless(p);
                               });
          })) {
    return false;
  }
  return true;
}

bool Grounder::simplify(Action &action) const noexcept {
  bool changed = false;
  if (auto it = std::remove_if(action.effects.begin(), action.effects.end(),
                               [this](const auto &p) {
                                 if (is_ground(p.atom)) {
                                   auto ground_atom = as_ground_atom(p.atom);
                                   return is_rigid(ground_atom, p.positive) ||
                                          is_useless(ground_atom);
                                 }
                                 return false;
                               });
      it != action.effects.end()) {
    action.effects.erase(it, action.effects.end());
    changed = true;
  }

  if (auto it = std::remove_if(action.preconditions.begin(),
                               action.preconditions.end(),
                               [this](const auto &p) {
                                 if (is_ground(p.atom)) {
                                   auto ground_atom = as_ground_atom(p.atom);
                                   return is_rigid(ground_atom, p.positive);
                                 }
                                 return false;
                               });
      it != action.preconditions.end()) {
    action.preconditions.erase(it, action.preconditions.end());
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
