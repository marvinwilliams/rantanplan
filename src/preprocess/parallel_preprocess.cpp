#include "preprocess/parallel_preprocess.hpp"
#include "build_config.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "model/to_string.hpp"
#include "planner/planner.hpp"
#include "util/timer.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <mutex>
#include <numeric>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace normalized;
using namespace std::chrono_literals;

ParallelPreprocessor::ParallelPreprocessor(
    unsigned int num_threads, const std::shared_ptr<Problem> &problem) noexcept
    : successful_cache_(problem->predicates.size()),
      unsuccessful_cache_(problem->predicates.size()), problem_{problem} {
  num_actions_ =
      std::accumulate(problem_->actions.begin(), problem_->actions.end(), 0ul,
                      [this](uint_fast64_t sum, const auto &a) {
                        return sum + get_num_instantiated(a, *problem_);
                      });

  trivially_rigid_.resize(problem_->predicates.size(), true);
  trivially_effectless_.resize(problem_->predicates.size(), true);
  for (const auto &action : problem_->actions) {
    for (const auto &[precondition, positive] : action.pre_instantiated) {
      trivially_effectless_[precondition.definition] = false;
    }
    for (const auto &precondition : action.preconditions) {
      trivially_effectless_[precondition.definition] = false;
    }
    for (const auto &[effect, positive] : action.eff_instantiated) {
      trivially_rigid_[effect.definition] = false;
    }
    for (const auto &effect : action.effects) {
      trivially_rigid_[effect.definition] = false;
    }
  }

  init_.resize(problem_->predicates.size());
  for (const auto &predicate : problem_->init) {
    init_[predicate.definition].push_back(get_id(predicate));
  }
  std::for_each(init_.begin(), init_.end(),
                [](auto &i) { std::sort(i.begin(), i.end()); });

  goal_.resize(problem_->predicates.size());
  for (const auto &[predicate, positive] : problem_->goal) {
    goal_[predicate.definition].emplace_back(get_id(predicate), positive);
  }
  std::for_each(goal_.begin(), goal_.end(),
                [](auto &g) { std::sort(g.begin(), g.end()); });

  actions_.reserve(problem_->actions.size());

  for (const auto &action : problem_->actions) {
    actions_.push_back({action});
  }

  prune_actions(num_threads);

  progress_ = static_cast<float>(get_num_actions() + num_pruned_actions_) /
              static_cast<float>(num_actions_);

  parameter_selector_ = std::invoke([mode = config.preprocess_mode]() {
    switch (mode) {
    case Config::PreprocessMode::New:
      return &ParallelPreprocessor::select_min_new;
    case Config::PreprocessMode::Rigid:
      return &ParallelPreprocessor::select_max_rigid;
    case Config::PreprocessMode::Free:
      return &ParallelPreprocessor::select_free;
    }
    return &ParallelPreprocessor::select_free;
  });
}

ParallelPreprocessor::Status ParallelPreprocessor::get_status() const noexcept {
  return status_;
}

void ParallelPreprocessor::refine(float progress, std::chrono::seconds timeout,
                                  unsigned int num_threads) noexcept {
  assert(status_ != Status::Interrupt);
  status_ = Status::Success;
  util::Timer timer;

  auto check_timeout = [&]() {
    if (config.check_timeout() ||
        (timeout > 0s && std::chrono::ceil<std::chrono::seconds>(
                             timer.get_elapsed_time()) >= timeout)) {
      return true;
    }
    return false;
  };

  while (progress_ < progress && status_ == Status::Success) {
    if (check_timeout()) {
      status_ = Status::Timeout;
      return;
    }
    for (auto &action_list : actions_) {
      std::vector<normalized::Action> new_actions;
      std::mutex new_actions_mutex;
      std::vector<std::thread> threads(num_threads);
      std::atomic_uint_fast64_t index_counter = 0;
      std::for_each(threads.begin(), threads.end(), [&](auto &t) {
        t = std::thread{[&]() {
          std::vector<normalized::Action> thread_new_actions;
          uint_fast64_t action_index;
          while ((action_index = index_counter.fetch_add(
                      1, std::memory_order_relaxed)) < action_list.size()) {
            if (config.global_stop_flag.load(std::memory_order_acquire)) {
              return;
            }
            const auto action = action_list[action_index];
            auto selection = std::invoke(parameter_selector_, *this, action);
            auto assignment_it =
                AssignmentIterator{selection, action, *problem_};
            thread_new_actions.reserve(thread_new_actions.size() +
                                       assignment_it.get_num_instantiations());
            std::for_each(assignment_it, AssignmentIterator{},
                          [&](const auto &assignment) {
                            if (auto new_action = ground(assignment, action);
                                is_valid(new_action)) {
                              simplify(new_action);
                              thread_new_actions.push_back(new_action);
                            } else {
                              num_pruned_actions_.fetch_add(
                                  get_num_instantiated(new_action, *problem_),
                                  std::memory_order_relaxed);
                            }
                          });
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
        status_ = Status::Interrupt;
        return;
      }
      action_list = std::move(new_actions);
      progress_ = static_cast<float>(get_num_actions() + num_pruned_actions_) /
                  static_cast<float>(num_actions_);
      if (progress_ >= progress) {
        break;
      }
    }
    prune_actions(num_threads);
  }
}

size_t ParallelPreprocessor::get_num_actions() const noexcept {
  uint_fast64_t sum = 0;
  for (const auto &actions : actions_) {
    sum += actions.size();
  }
  return sum;
}

float ParallelPreprocessor::get_progress() const noexcept { return progress_; }

ParallelPreprocessor::PredicateId
ParallelPreprocessor::get_id(const PredicateInstantiation &predicate) const
    noexcept {
  uint_fast64_t result = 0;
  for (auto arg : predicate.arguments) {
    result = (result * problem_->constants.size()) + arg;
  }
  return result;
}

bool ParallelPreprocessor::is_trivially_rigid(
    const PredicateInstantiation &predicate, bool positive) const noexcept {
  if (std::binary_search(init_[predicate.definition].begin(),
                         init_[predicate.definition].end(),
                         get_id(predicate)) != positive) {
    return false;
  }
  return trivially_rigid_[predicate.definition];
}

bool ParallelPreprocessor::is_trivially_effectless(
    const PredicateInstantiation &predicate, bool positive) const noexcept {
  if (std::binary_search(goal_[predicate.definition].begin(),
                         goal_[predicate.definition].end(),
                         std::make_pair(get_id(predicate), positive))) {
    return false;
  }
  return trivially_effectless_[predicate.definition];
}

bool ParallelPreprocessor::has_effect(const Action &action,
                                      const PredicateInstantiation &predicate,
                                      bool positive) const noexcept {
  for (const auto &[effect, eff_positive] : action.eff_instantiated) {
    if (effect == predicate && eff_positive == positive) {
      return true;
    }
  }
  for (const auto &effect : action.effects) {
    if (effect.definition == predicate.definition &&
        effect.positive == positive) {
      if (is_instantiatable(effect, predicate.arguments, action, *problem_)) {
        return true;
      }
    }
  }
  return false;
}

bool ParallelPreprocessor::has_precondition(
    const Action &action, const PredicateInstantiation &predicate,
    bool positive) const noexcept {
  for (const auto &[precondition, pre_positive] : action.pre_instantiated) {
    if (precondition == predicate && pre_positive == positive) {
      return true;
    }
  }
  for (const auto &precondition : action.preconditions) {
    if (precondition.definition == predicate.definition &&
        precondition.positive == positive) {
      if (is_instantiatable(precondition, predicate.arguments, action,
                            *problem_)) {
        return true;
      }
    }
  }
  return false;
}

// No action has this predicate as effect and it is not in init_
bool ParallelPreprocessor::is_rigid(const PredicateInstantiation &predicate,
                                    bool positive) const noexcept {
  auto &rigid = (positive ? successful_cache_[predicate.definition].pos_rigid
                          : successful_cache_[predicate.definition].neg_rigid);
  auto &rigid_mutex =
      (positive ? successful_cache_[predicate.definition].pos_rigid_mutex
                : successful_cache_[predicate.definition].neg_rigid_mutex);
  auto &not_rigid =
      (positive ? unsuccessful_cache_[predicate.definition].pos_rigid
                : unsuccessful_cache_[predicate.definition].neg_rigid);
  auto &not_rigid_mutex =
      (positive ? unsuccessful_cache_[predicate.definition].pos_rigid_mutex
                : unsuccessful_cache_[predicate.definition].neg_rigid_mutex);
  auto id = get_id(predicate);

  if (std::lock_guard l{rigid_mutex}; rigid.find(id) != rigid.end()) {
    return true;
  }

  if (std::lock_guard l{not_rigid_mutex};
      not_rigid.find(id) != not_rigid.end()) {
    return false;
  }

  if (std::binary_search(init_[predicate.definition].begin(),
                         init_[predicate.definition].end(), id) != positive) {
    std::lock_guard l{not_rigid_mutex};
    not_rigid.insert(id);
    return false;
  }

  if (trivially_rigid_[predicate.definition]) {
    std::lock_guard l{rigid_mutex};
    rigid.insert(id);
    return true;
  }

  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    const auto &base_action = problem_->actions[i];
    if (!has_effect(base_action, predicate, !positive)) {
      continue;
    }
    for (const auto &action : actions_[i]) {
      if (has_effect(action, predicate, !positive)) {
        std::lock_guard l{not_rigid_mutex};
        not_rigid.insert(id);
        return false;
      }
    }
  }
  std::lock_guard l{rigid_mutex};
  rigid.insert(id);
  return true;
}

// No action has this predicate as precondition and it is a not a goal
bool ParallelPreprocessor::is_effectless(
    const PredicateInstantiation &predicate, bool positive) const noexcept {
  auto &effectless =
      (positive ? successful_cache_[predicate.definition].pos_effectless
                : successful_cache_[predicate.definition].neg_effectless);
  auto &effectless_mutex =
      (positive ? successful_cache_[predicate.definition].pos_effectless_mutex
                : successful_cache_[predicate.definition].neg_effectless_mutex);
  auto &not_effectless =
      (positive ? unsuccessful_cache_[predicate.definition].pos_effectless
                : unsuccessful_cache_[predicate.definition].neg_effectless);
  auto &not_effectless_mutex =
      (positive
           ? unsuccessful_cache_[predicate.definition].pos_effectless_mutex
           : unsuccessful_cache_[predicate.definition].neg_effectless_mutex);
  auto id = get_id(predicate);

  if (std::lock_guard l{effectless_mutex};
      effectless.find(id) != effectless.end()) {
    return true;
  }

  if (std::lock_guard l{not_effectless_mutex};
      not_effectless.find(id) != not_effectless.end()) {
    return false;
  }

  if (std::binary_search(goal_[predicate.definition].begin(),
                         goal_[predicate.definition].end(),
                         std::make_pair(id, positive))) {
    std::lock_guard l{not_effectless_mutex};
    not_effectless.insert(id);
    return false;
  }

  if (trivially_effectless_[predicate.definition]) {
    std::lock_guard l{effectless_mutex};
    effectless.insert(id);
    return true;
  }

  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    const auto &action = problem_->actions[i];
    if (!has_precondition(action, predicate, positive)) {
      continue;
    }
    for (const auto &action : actions_[i]) {
      if (has_precondition(action, predicate, positive)) {
        std::lock_guard l{not_effectless_mutex};
        not_effectless.insert(id);
        return false;
      }
    }
  }
  std::lock_guard l{effectless_mutex};
  effectless.insert(id);
  return true;
}

ParameterSelection ParallelPreprocessor::select_free(const Action &action) const
    noexcept {
  auto first_free =
      std::find_if(action.parameters.begin(), action.parameters.end(),
                   [](const auto &p) { return !p.is_constant(); });

  if (first_free == action.parameters.end()) {
    return ParameterSelection{};
  }

  return ParameterSelection{{first_free - action.parameters.begin()}};
}

ParameterSelection
ParallelPreprocessor::select_min_new(const Action &action) const noexcept {
  auto min =
      std::min_element(action.preconditions.begin(), action.preconditions.end(),
                       [&](const auto &c1, const auto &c2) {
                         return get_num_instantiated(c1, action, *problem_) <
                                get_num_instantiated(c2, action, *problem_);
                       });

  if (min == action.preconditions.end()) {
    return select_free(action);
  }

  return get_referenced_parameters(action, *min);
}

ParameterSelection
ParallelPreprocessor::select_max_rigid(const Action &action) const noexcept {
  auto max = std::max_element(
      action.preconditions.begin(), action.preconditions.end(),
      [this, &action](const auto &c1, const auto &c2) {
        return std::count_if(
                   ConditionIterator{c1, action, *problem_},
                   ConditionIterator{},
                   [&](const auto p) { return is_rigid(p, !c1.positive); }) <
               std::count_if(
                   ConditionIterator{c2, action, *problem_},
                   ConditionIterator{},
                   [&](const auto p) { return is_rigid(p, !c2.positive); });
      });

  if (max == action.preconditions.end()) {
    return select_free(action);
  }

  return get_referenced_parameters(action, *max);
}

void ParallelPreprocessor::prune_actions(unsigned int num_threads) noexcept {
  std::atomic_bool changed;
  do {
    changed = false;
    std::for_each(unsuccessful_cache_.begin(), unsuccessful_cache_.end(),
                  [](auto &c) {
                    c.pos_rigid.clear();
                    c.neg_rigid.clear();
                    c.pos_effectless.clear();
                    c.neg_effectless.clear();
                  });
    for (auto &action_list : actions_) {
      std::vector<std::atomic_bool> remove_action(action_list.size());
      std::fill(remove_action.begin(), remove_action.end(), false);
      std::vector<std::thread> threads(num_threads);
      std::atomic_uint_fast64_t index_counter = 0;
      std::for_each(threads.begin(), threads.end(), [&](auto &t) {
        t = std::thread{[&]() {
          uint_fast64_t action_index;
          while ((action_index = index_counter.fetch_add(
                      1, std::memory_order_relaxed)) < action_list.size()) {
            if (config.global_stop_flag.load(std::memory_order_acquire)) {
              return;
            }
            auto &action = action_list[action_index];
            if (is_valid(action)) {
              if (simplify(action)) {
                changed = true;
              }
            } else {
              remove_action[action_index] = true;
              num_pruned_actions_ += get_num_instantiated(action, *problem_);
              changed = true;
            }
          }
        }};
      });
      std::for_each(threads.begin(), threads.end(), [](auto &t) { t.join(); });
      if (config.global_stop_flag.load(std::memory_order_acquire)) {
        status_ = Status::Interrupt;
        return;
      }
      action_list.erase(
          std::remove_if(action_list.begin(), action_list.end(),
                         [&](const auto &a) {
                           return remove_action[static_cast<size_t>(
                                                    &a - &*action_list.begin())]
                               .load();
                         }),
          action_list.end());
    }
  } while (changed);
}

bool ParallelPreprocessor::is_valid(const Action &action) const noexcept {
  if (std::any_of(
          action.pre_instantiated.begin(), action.pre_instantiated.end(),
          [this](const auto &p) { return is_rigid(p.first, !p.second); })) {
    return false;
  }

  if (action.effects.empty() &&
      std::all_of(action.eff_instantiated.begin(),
                  action.eff_instantiated.end(), [this](const auto &p) {
                    return is_rigid(p.first, p.second) ||
                           is_effectless(p.first, p.second);
                  })) {
    return false;
  }
  return true;
}

bool ParallelPreprocessor::simplify(Action &action) const noexcept {
  bool changed = false;
  if (auto it = std::remove_if(action.eff_instantiated.begin(),
                               action.eff_instantiated.end(),
                               [this](const auto &p) {
                                 return is_rigid(p.first, p.second) ||
                                        (is_effectless(p.first, p.second) &&
                                         is_effectless(p.first, !p.second));
                               });
      it != action.eff_instantiated.end()) {
    action.eff_instantiated.erase(it, action.eff_instantiated.end());
    changed = true;
  }

  if (auto it = std::remove_if(
          action.pre_instantiated.begin(), action.pre_instantiated.end(),
          [this](const auto &p) { return is_rigid(p.first, p.second); });
      it != action.pre_instantiated.end()) {
    action.pre_instantiated.erase(it, action.pre_instantiated.end());
    changed = true;
  }
  return changed;
}

std::shared_ptr<Problem> ParallelPreprocessor::extract_problem() const
    noexcept {
  auto preprocessed_problem = std::make_shared<Problem>();
  preprocessed_problem->domain_name = problem_->domain_name;
  preprocessed_problem->problem_name = problem_->problem_name;
  preprocessed_problem->requirements = problem_->requirements;
  preprocessed_problem->types = problem_->types;
  preprocessed_problem->type_names = problem_->type_names;
  preprocessed_problem->constants = problem_->constants;
  preprocessed_problem->constant_names = problem_->constant_names;
  preprocessed_problem->constants_by_type = problem_->constants_by_type;
  preprocessed_problem->predicates = problem_->predicates;
  preprocessed_problem->predicate_names = problem_->predicate_names;
  preprocessed_problem->init = problem_->init;
  std::copy_if(problem_->goal.begin(), problem_->goal.end(),
               std::back_inserter(preprocessed_problem->goal),
               [this](const auto &g) { return !is_rigid(g.first, g.second); });

  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    for (auto &action : actions_[i]) {
      preprocessed_problem->actions.push_back(std::move(action));
      preprocessed_problem->action_names.push_back(problem_->action_names[i]);
    }
  }

  return preprocessed_problem;
}
