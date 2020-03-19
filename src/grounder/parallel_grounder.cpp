#include "grounder/parallel_grounder.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "planner/planner.hpp"
#include "util/timer.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace normalized;

ParallelGrounder::ParallelGrounder(
    unsigned int num_threads, const std::shared_ptr<Problem> &problem) noexcept
    : trivially_rigid_(problem->predicates.size(), true),
      trivially_useless_(problem->predicates.size(), true),
      init_(problem->predicates.size()), goal_(problem->predicates.size()),
      action_grounded_(problem->actions.size(), false),
      successful_cache_(problem->predicates.size()),
      unsuccessful_cache_(problem->predicates.size()), problem_{problem} {
  num_actions_ =
      std::accumulate(problem_->actions.begin(), problem_->actions.end(), 0ul,
                      [this](uint_fast64_t sum, const auto &a) {
                        return sum + get_num_instantiated(a, *problem_);
                      });

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
      trivially_rigid_[effect.predicate] = false;
    }
  }

  for (const auto &init : problem_->init) {
    init_[init.predicate].push_back(get_id(init));
  }
  for (auto &i : init_) {
    std::sort(i.begin(), i.end());
  }

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

  prune_actions(num_threads);

  groundness_ = static_cast<float>(get_num_actions() + num_pruned_actions_) /
                static_cast<float>(num_actions_);

  parameter_selector_ = std::invoke([]() {
    switch (config.parameter_selection) {
    case Config::ParameterSelection::MostFrequent:
      return &ParallelGrounder::select_most_frequent;
    case Config::ParameterSelection::MinNew:
      return &ParallelGrounder::select_min_new;
    case Config::ParameterSelection::MaxRigid:
      return &ParallelGrounder::select_max_rigid;
    case Config::ParameterSelection::ApproxMinNew:
      return &ParallelGrounder::select_approx_min_new;
    case Config::ParameterSelection::ApproxMaxRigid:
      return &ParallelGrounder::select_approx_max_rigid;
    case Config::ParameterSelection::FirstEffect:
      return &ParallelGrounder::select_first_effect;
    }
    return &ParallelGrounder::select_approx_max_rigid;
  });
}

bool ParallelGrounder::is_rigid(const GroundAtom &atom, bool positive) const
    noexcept {
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

bool ParallelGrounder::is_useless(const GroundAtom &atom) const noexcept {
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

void ParallelGrounder::refine(float groundness, util::Seconds timeout,
                              unsigned int num_threads) {
  util::Timer timer;

  while (groundness_ < groundness) {
    LOG_INFO(grounder_logger, "Current groundness: %.3f", groundness_);
    LOG_INFO(grounder_logger, "Current actions: %lu actions",
             get_num_actions());
    std::atomic_bool keep_grounding = false;
    for (size_t i = 0; i < actions_.size(); ++i) {
      if (action_grounded_[i]) {
        continue;
      }
      std::atomic_bool action_grounded = true;
      std::vector<Action> new_actions;
      std::mutex new_actions_mutex;
      std::atomic_uint_fast64_t new_pruned_actions = 0;
      std::vector<std::thread> threads(num_threads);
      std::atomic_uint_fast64_t index_counter = 0;
      std::for_each(threads.begin(), threads.end(), [&](auto &t) {
        t = std::thread{[&]() {
          std::vector<normalized::Action> thread_new_actions;
          uint_fast64_t action_index;
          while ((action_index = index_counter.fetch_add(
                      1, std::memory_order_relaxed)) < actions_[i].size()) {
            if (config.global_stop_flag.load(std::memory_order_acquire)) {
              return;
            }
            if (timeout != util::inf_time &&
                timer.get_elapsed_time() > timeout) {
              return;
            }
            if (config.timeout != util::inf_time &&
                global_timer.get_elapsed_time() > config.timeout) {
              throw TimeoutException{};
            }
            const auto &action = actions_[i][action_index];
            auto selection = std::invoke(parameter_selector_, *this, action);
            if (!selection.empty()) {
              action_grounded = false;
            }
            for (auto it = AssignmentIterator{selection, action, *problem_};
                 it != AssignmentIterator{}; ++it) {
              auto [new_action, valid] = ground(action, *it);
              if (valid) {
                thread_new_actions.push_back(new_action);
              } else {
                new_pruned_actions.fetch_add(
                    get_num_instantiated(new_action, *problem_),
                    std::memory_order_relaxed);
              }
            };
          }
          std::lock_guard l{new_actions_mutex};
          new_actions.insert(
              new_actions.end(),
              std::make_move_iterator(thread_new_actions.begin()),
              std::make_move_iterator(thread_new_actions.end()));
        }};
      });
      std::for_each(threads.begin(), threads.end(), [](auto &t) { t.join(); });
      if (config.global_stop_flag.load(std::memory_order_acquire)) {
        return;
      }
      if (action_grounded) {
        action_grounded_[i] = true;
      } else {
        keep_grounding = true;
      }
      num_pruned_actions_ += new_pruned_actions;
      actions_[i] = std::move(new_actions);
      groundness_ =
          static_cast<float>(get_num_actions() + num_pruned_actions_) /
          static_cast<float>(num_actions_);
      if (groundness_ >= groundness) {
        break;
      }
    }
    if (!keep_grounding) {
      return;
    }
    prune_actions(num_threads);
  }
}

size_t ParallelGrounder::get_num_actions() const noexcept {
  uint_fast64_t sum = 0;
  for (const auto &actions : actions_) {
    sum += actions.size();
  }
  return sum;
}

float ParallelGrounder::get_groundness() const noexcept { return groundness_; }

ParallelGrounder::PredicateId
ParallelGrounder::get_id(const GroundAtom &atom) const noexcept {
  uint_fast64_t result = 0;
  for (auto arg : atom.arguments) {
    result = (result * problem_->constants.size()) + arg;
  }
  return result;
}

bool ParallelGrounder::is_trivially_rigid(const GroundAtom &atom,
                                          bool positive) const noexcept {
  if (std::binary_search(init_[atom.predicate].begin(),
                         init_[atom.predicate].end(),
                         get_id(atom)) != positive) {
    return false;
  }
  return trivially_rigid_[atom.predicate];
}

bool ParallelGrounder::is_trivially_useless(const GroundAtom &atom) const
    noexcept {
  if (std::binary_search(goal_[atom.predicate].begin(),
                         goal_[atom.predicate].end(), get_id(atom))) {
    return false;
  }
  return trivially_useless_[atom.predicate];
}

bool ParallelGrounder::has_precondition(const Action &action,
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

bool ParallelGrounder::has_effect(const Action &action, const GroundAtom &atom,
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

ParameterSelection
ParallelGrounder::select_most_frequent(const Action &action) const noexcept {
  if (action.parameters.empty()) {
    return {};
  }

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

ParameterSelection ParallelGrounder::select_min_new(const Action &action) const
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

ParameterSelection
ParallelGrounder::select_max_rigid(const Action &action) const noexcept {
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

ParameterSelection
ParallelGrounder::select_approx_min_new(const Action &action) const noexcept {
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

ParameterSelection
ParallelGrounder::select_approx_max_rigid(const Action &action) const noexcept {
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

ParameterSelection
ParallelGrounder::select_first_effect(const Action &action) const noexcept {
  if (action.effects.empty()) {
    return select_most_frequent(action);
  } else {
    return get_referenced_parameters(action.effects[0].atom, action);
  }
}

void ParallelGrounder::prune_actions(unsigned int num_threads) noexcept {
  std::atomic_bool changed;
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
      std::vector<Action> new_actions;
      std::mutex new_actions_mutex;
      std::vector<std::thread> threads(num_threads);
      std::atomic_uint_fast64_t new_pruned_actions = 0;
      std::atomic_uint_fast64_t index_counter = 0;
      std::for_each(threads.begin(), threads.end(), [&](auto &t) {
        t = std::thread{[&]() {
          uint_fast64_t action_index;
          while ((action_index = index_counter.fetch_add(
                      1, std::memory_order_relaxed)) < actions_[i].size()) {
            const auto &action = actions_[i][action_index];
            if (is_valid(action)) {
              auto new_action = action;
              if (simplify(new_action)) {
                changed = true;
              }
              std::lock_guard l{new_actions_mutex};
              new_actions.push_back(std::move(new_action));
            } else {
              new_pruned_actions += get_num_instantiated(action, *problem_);
              changed = true;
            }
          }
        }};
      });
      std::for_each(threads.begin(), threads.end(), [](auto &t) { t.join(); });
      actions_[i] = std::move(new_actions);
      num_pruned_actions_ += new_pruned_actions;
    }
  } while (changed);
}

bool ParallelGrounder::is_valid(const Action &action) const noexcept {
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

std::pair<normalized::Action, bool> ParallelGrounder::ground(
    const normalized::Action &action,
    const normalized::ParameterAssignment &assignment) const noexcept {
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

bool ParallelGrounder::simplify(Action &action) const noexcept {
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

std::shared_ptr<Problem> ParallelGrounder::extract_problem() const noexcept {
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
