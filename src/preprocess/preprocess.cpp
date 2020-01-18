#include "preprocess/preprocess.hpp"
#include "build_config.hpp"
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

Preprocessor::Preprocessor(const std::shared_ptr<Problem> &problem,
                           const Config &config) noexcept
    : config_{config}, problem_{problem} {
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

  simplify_actions();

  progress_ = static_cast<float>(get_num_actions() + num_pruned_actions_) /
              static_cast<float>(num_actions_);

  parameter_selector_ = std::invoke([mode = config_.preprocess_mode]() {
    switch (mode) {
    case Config::PreprocessMode::New:
      return &Preprocessor::select_min_new;
    case Config::PreprocessMode::Rigid:
      return &Preprocessor::select_max_rigid;
    case Config::PreprocessMode::Free:
      return &Preprocessor::select_free;
    }
    return &Preprocessor::select_free;
  });
}

bool Preprocessor::refine(float progress) noexcept {
  while (progress_ < progress) {
    for (auto action_list = actions_.begin(); action_list != actions_.end();
         ++action_list) {
      if (config_.timeout > 0s &&
          std::chrono::ceil<std::chrono::seconds>(
              util::global_timer.get_elapsed_time()) >= config_.timeout) {
        return false;
      }
      std::vector<Action> new_actions;
      for (auto a = action_list->begin(); a != action_list->end(); ++a) {
        auto selection = std::invoke(parameter_selector_, *this, *a);
        for_each_instantiation(
            selection, *a,
            [&](const auto &assignment) {
              if (auto new_action = ground(assignment, *a);
                  is_valid(new_action)) {
                simplify(new_action);
                new_actions.push_back(new_action);
              } else {
                num_pruned_actions_ +=
                    get_num_instantiated(new_action, *problem_);
              }
            },
            *problem_);
      }
      *action_list = std::move(new_actions);
    }
    simplify_actions();
    progress_ = static_cast<float>(get_num_actions() + num_pruned_actions_) /
                static_cast<float>(num_actions_);
  }
  return true;
}

size_t Preprocessor::get_num_actions() const noexcept {
  uint_fast64_t sum = 0;
  for (const auto &actions : actions_) {
    sum += actions.size();
  }
  return sum;
}

float Preprocessor::get_progress() const noexcept { return progress_; }

Preprocessor::PredicateId
Preprocessor::get_id(const PredicateInstantiation &predicate) const noexcept {
  uint_fast64_t result = 0;
  for (auto arg : predicate.arguments) {
    result = (result * problem_->constants.size()) + arg;
  }
  return result;
}

bool Preprocessor::is_trivially_rigid(const PredicateInstantiation &predicate,
                                      bool positive) const noexcept {
  if (std::binary_search(init_[predicate.definition].begin(),
                         init_[predicate.definition].end(),
                         get_id(predicate)) != positive) {
    return false;
  }
  return trivially_rigid_[predicate.definition];
}

bool Preprocessor::is_trivially_effectless(
    const PredicateInstantiation &predicate, bool positive) const noexcept {
  if (std::binary_search(goal_[predicate.definition].begin(),
                         goal_[predicate.definition].end(),
                         std::make_pair(get_id(predicate), positive))) {
    return false;
  }
  return trivially_effectless_[predicate.definition];
}

bool Preprocessor::has_effect(const Action &action,
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

bool Preprocessor::has_precondition(const Action &action,
                                    const PredicateInstantiation &predicate,
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
bool Preprocessor::is_rigid(const PredicateInstantiation &predicate,
                            bool positive) const noexcept {
  auto id = get_id(predicate);
  if (std::binary_search(init_[predicate.definition].begin(),
                         init_[predicate.definition].end(), id) != positive) {
    return false;
  }

  if (trivially_rigid_[predicate.definition]) {
    return true;
  }

  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    const auto &base_action = problem_->actions[i];
    if (!has_effect(base_action, predicate, !positive)) {
      continue;
    }
    for (const auto &action : actions_[i]) {
      if (has_effect(action, predicate, !positive)) {
        return false;
      }
    }
  }
  return true;
}

// No action has this predicate as precondition and it is a not a goal
bool Preprocessor::is_effectless(const PredicateInstantiation &predicate,
                                 bool positive) const noexcept {
  auto id = get_id(predicate);

  if (std::binary_search(goal_[predicate.definition].begin(),
                         goal_[predicate.definition].end(),
                         std::make_pair(id, positive))) {
    return false;
  }

  if (trivially_effectless_[predicate.definition]) {
    return true;
  }

  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    const auto &action = problem_->actions[i];
    if (!has_precondition(action, predicate, positive)) {
      continue;
    }
    for (const auto &action : actions_[i]) {
      if (has_precondition(action, predicate, positive)) {
        return false;
      }
    }
  }
  return true;
}

ParameterSelection Preprocessor::select_free(const Action &action) const
    noexcept {
  auto first_free =
      std::find_if(action.parameters.begin(), action.parameters.end(),
                   [](const auto &p) { return !p.is_constant(); });

  if (first_free == action.parameters.end()) {
    return ParameterSelection{};
  }

  return ParameterSelection{{first_free - action.parameters.begin()}};
}

ParameterSelection Preprocessor::select_min_new(const Action &action) const
    noexcept {
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

ParameterSelection Preprocessor::select_max_rigid(const Action &action) const
    noexcept {
  auto max =
      std::max_element(action.preconditions.begin(), action.preconditions.end(),
                       [this, &action](const auto &c1, const auto &c2) {
                         uint_fast64_t c1_pruned = 0;
                         uint_fast64_t c2_pruned = 0;
                         for_each_instantiation(
                             c1, action,
                             [&](const auto &new_condition, const auto &) {
                               if (is_rigid(new_condition, !c1.positive)) {
                                 ++c1_pruned;
                               }
                             },
                             *problem_);
                         for_each_instantiation(
                             c2, action,
                             [&](const auto &new_condition, const auto &) {
                               if (is_rigid(new_condition, !c2.positive)) {
                                 ++c2_pruned;
                               }
                             },
                             *problem_);
                         return c1_pruned < c2_pruned;
                       });

  if (max == action.preconditions.end()) {
    return select_free(action);
  }

  return get_referenced_parameters(action, *max);
}

void Preprocessor::simplify_actions() noexcept {
  bool changed;
  do {
    changed = false;
    for (size_t i = 0; i < actions_.size(); ++i) {
      if (auto it =
              std::partition(actions_[i].begin(), actions_[i].end(),
                             [this](const auto &a) { return is_valid(a); });
          it != actions_[i].end()) {
        std::for_each(it, actions_[i].end(), [this](const auto &a) {
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

bool Preprocessor::is_valid(const Action &action) const noexcept {
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

bool Preprocessor::simplify(Action &action) const noexcept {
  bool changed = false;
  if (auto it = std::remove_if(
          action.eff_instantiated.begin(), action.eff_instantiated.end(),
          [this](const auto &p) { return is_rigid(p.first, p.second); });
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

std::shared_ptr<Problem> Preprocessor::extract_problem() const noexcept {
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
