#include "preprocess/preprocess.hpp"
#include "build_config.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "model/to_string.hpp"
#include "planner/planner.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace normalized;

Preprocessor::Preprocessor(const std::shared_ptr<Problem> &problem,
                           const Config &config) noexcept
    : config_{config}, problem_{problem} {
  num_actions_ =
      std::accumulate(problem_->actions.begin(), problem_->actions.end(), 0ul,
                      [this](uint_fast64_t sum, const auto &a) {
                        return sum + get_num_instantiated(a, *problem_);
                      });

  partially_instantiated_actions_.reserve(problem_->actions.size());

  for (const auto &action : problem_->actions) {
    partially_instantiated_actions_.push_back({action});
  }

  predicate_id_offset_.reserve(problem_->predicates.size() + 1);
  predicate_id_offset_.push_back(0);

  for (auto it = problem_->predicates.begin(); it != problem_->predicates.end();
       ++it) {
    predicate_id_offset_.push_back(
        predicate_id_offset_.back() +
        static_cast<uint_fast64_t>(
            std::pow(problem_->constants.size(), it->parameter_types.size())));
  }

  trivially_rigid_.resize(problem_->predicates.size(), true);
  trivially_effectless_.resize(problem_->predicates.size(), true);

  pos_precondition_support_.resize(predicate_id_offset_.back(), 0ul);
  neg_precondition_support_.resize(predicate_id_offset_.back(), 0ul);
  pos_effect_support_.resize(predicate_id_offset_.back(), 0ul);
  neg_effect_support_.resize(predicate_id_offset_.back(), 0ul);

  for (const auto &action : problem_->actions) {
    uint_fast64_t num_action_instantiations =
        get_num_instantiated(action, *problem_);
    for (const auto &[precondition, positive] : action.pre_instantiated) {
      trivially_effectless_[precondition.definition] = false;
      (positive ? pos_precondition_support_
                : neg_precondition_support_)[get_id(precondition)] +=
          num_action_instantiations;
    }
    for (const auto &precondition : action.preconditions) {
      trivially_effectless_[precondition.definition] = false;
      uint_fast64_t num_instantiations =
          num_action_instantiations /
          get_num_instantiated(precondition, action, *problem_);
      for_each_instantiation(
          precondition, action,
          [&](const auto &new_condition, const auto &) {
            (precondition.positive
                 ? pos_precondition_support_
                 : neg_precondition_support_)[get_id(new_condition)] +=
                num_instantiations;
          },
          *problem_);
    }
    for (const auto &[effect, positive] : action.eff_instantiated) {
      trivially_rigid_[effect.definition] = false;
      (positive ? pos_effect_support_ : neg_effect_support_)[get_id(effect)] +=
          num_action_instantiations;
    }
    for (const auto &effect : action.effects) {
      trivially_rigid_[effect.definition] = false;
      uint_fast64_t num_instantiations =
          num_action_instantiations /
          get_num_instantiated(effect, action, *problem_);
      for_each_instantiation(
          effect, action,
          [&](const auto &new_condition, const auto &) {
            (effect.positive ? pos_effect_support_
                             : neg_effect_support_)[get_id(new_condition)] +=
                num_instantiations;
          },
          *problem_);
    }
  }

  init_.reserve(problem_->init.size());
  for (const auto &predicate : problem_->init) {
    init_.push_back(get_id(predicate));
  }
  std::sort(init_.begin(), init_.end());

  goal_.reserve(problem_->goal.size());
  for (const auto &[predicate, positive] : problem_->goal) {
    auto id = get_id(predicate);
    goal_.emplace_back(id, positive);
    (positive ? pos_precondition_support_ : neg_precondition_support_)[id] += 1;
  }
  std::sort(goal_.begin(), goal_.end());

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

bool Preprocessor::refine() noexcept {
  bool refinement_possible = false;

  for (size_t action_index = 0; action_index < problem_->actions.size();
       ++action_index) {
    std::vector<normalized::Action> new_actions;
    for (size_t i = 0; i < partially_instantiated_actions_[action_index].size();
         ++i) {
      auto &action = partially_instantiated_actions_[action_index][i];
      if (auto result = simplify(action); result != SimplifyResult::Unchanged) {
        refinement_possible = true;
        if (result == SimplifyResult::Invalid) {
          num_pruned_actions_ +=
              normalized::get_num_instantiated(action, *problem_);
          continue;
        }
      }

      auto selection = std::invoke(parameter_selector_, *this, action);

      if (selection.empty()) {
        new_actions.push_back(action);
        continue;
      }

      refinement_possible = true;

      for_each_instantiation(
          selection, action,
          [&](const auto &assignment) {
            auto new_action = ground(assignment, action);
            if (auto result = simplify(new_action);
                result != SimplifyResult::Invalid) {
              new_actions.push_back(std::move(new_action));
            } else {
              num_pruned_actions_ +=
                  normalized::get_num_instantiated(new_action, *problem_);
            }
          },
          *problem_);
    }
    partially_instantiated_actions_[action_index] = std::move(new_actions);
  }
  return refinement_possible;
}

size_t Preprocessor::get_num_actions() const noexcept {
  uint_fast64_t sum = 0;
  for (const auto &actions : partially_instantiated_actions_) {
    sum += actions.size();
  }
  return sum;
}

float Preprocessor::get_progress() const noexcept {
  return static_cast<float>(get_num_actions() + num_pruned_actions_) /
         static_cast<float>(num_actions_);
}

Preprocessor::PredicateId
Preprocessor::get_id(const PredicateInstantiation &predicate) const noexcept {
  uint_fast64_t result = 0;
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    result = (result * problem_->constants.size()) + predicate.arguments[i];
  }
  result += predicate_id_offset_[predicate.definition];
  return result;
}

bool Preprocessor::is_trivially_rigid(const PredicateInstantiation &predicate,
                                      bool positive) const noexcept {
  if (std::binary_search(init_.begin(), init_.end(), get_id(predicate)) !=
      positive) {
    return false;
  }
  return trivially_rigid_[predicate.definition];
}

bool Preprocessor::is_trivially_effectless(
    const PredicateInstantiation &predicate, bool positive) const noexcept {
  if (std::binary_search(goal_.begin(), goal_.end(),
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

// No action has this predicate negated as effect and it is not in init
bool Preprocessor::is_rigid(const PredicateInstantiation &predicate,
                            bool positive) const noexcept {
  auto id = get_id(predicate);
  if (std::binary_search(init_.begin(), init_.end(), id) != positive) {
    return false;
  }

  return (positive ? neg_effect_support_ : pos_effect_support_)[id] == 0;
}

// No action has this predicate as precondition and it is a not a goal
bool Preprocessor::is_effectless(const PredicateInstantiation &predicate,
                                 bool positive) const noexcept {
  return (positive ? pos_precondition_support_
                   : neg_precondition_support_)[get_id(predicate)] == 0;
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

void Preprocessor::remove_action(const Action &action) const noexcept {
  uint_fast64_t num_action_instantiations =
      get_num_instantiated(action, *problem_);

  for (const auto &[precondition, positive] : action.pre_instantiated) {
    assert((positive ? pos_precondition_support_
                     : neg_precondition_support_)[get_id(precondition)] >=
           num_action_instantiations);
    (positive ? pos_precondition_support_
              : neg_precondition_support_)[get_id(precondition)] -=
        num_action_instantiations;
  }

  for (const auto &precondition : action.preconditions) {
    uint_fast64_t num_instantiations =
        num_action_instantiations /
        get_num_instantiated(precondition, action, *problem_);
    for_each_instantiation(
        precondition, action,
        [&](const auto &new_condition, const auto &) {
          assert((precondition.positive
                      ? pos_precondition_support_
                      : neg_precondition_support_)[get_id(new_condition)] >=
                 num_instantiations);
          (precondition.positive
               ? pos_precondition_support_
               : neg_precondition_support_)[get_id(new_condition)] -=
              num_instantiations;
        },
        *problem_);
  }

  for (const auto &[effect, positive] : action.eff_instantiated) {
    assert((positive ? pos_effect_support_
                     : neg_effect_support_)[get_id(effect)] >=
           num_action_instantiations);
    (positive ? pos_effect_support_ : neg_effect_support_)[get_id(effect)] -=
        num_action_instantiations;
  }

  for (const auto &effect : action.effects) {
    uint_fast64_t num_instantiations =
        num_action_instantiations /
        get_num_instantiated(effect, action, *problem_);
    for_each_instantiation(
        effect, action,
        [&](const auto &new_condition, const auto &) {
          assert((effect.positive
                      ? pos_effect_support_
                      : neg_effect_support_)[get_id(new_condition)] >=
                 num_instantiations);
          (effect.positive ? pos_effect_support_
                           : neg_effect_support_)[get_id(new_condition)] -=
              num_instantiations;
        },
        *problem_);
  }
}

Preprocessor::SimplifyResult Preprocessor::simplify(Action &action) const
    noexcept {
  uint_fast64_t num_action_instantiations =
      get_num_instantiated(action, *problem_);

  if (std::any_of(
          action.pre_instantiated.begin(), action.pre_instantiated.end(),
          [this](const auto &p) { return is_rigid(p.first, !p.second); })) {
    remove_action(action);
    return SimplifyResult::Invalid;
  }

  if (action.effects.empty() &&
      std::all_of(action.eff_instantiated.begin(),
                  action.eff_instantiated.end(), [this](const auto &p) {
                    return is_rigid(p.first, p.second) ||
                           is_effectless(p.first, p.second);
                  })) {
    remove_action(action);
    return SimplifyResult::Invalid;
  }

  SimplifyResult result = SimplifyResult::Unchanged;

  if (auto it = std::partition(action.eff_instantiated.begin(),
                               action.eff_instantiated.end(),
                               [&](const auto &p) {
                                 return !(is_rigid(p.first, p.second) ||
                                          (is_effectless(p.first, p.second) &&
                                           is_effectless(p.first, !p.second)));
                               });
      it != action.eff_instantiated.end()) {
    std::for_each(it, action.eff_instantiated.end(), [&](const auto &effect) {
      assert((effect.second ? pos_effect_support_
                            : neg_effect_support_)[get_id(effect.first)] >=
             num_action_instantiations);
      (effect.second ? pos_effect_support_
                     : neg_effect_support_)[get_id(effect.first)] -=
          num_action_instantiations;
    });
    action.eff_instantiated.erase(it, action.eff_instantiated.end());
    result = SimplifyResult::Changed;
  }

  if (auto it = std::partition(
          action.pre_instantiated.begin(), action.pre_instantiated.end(),
          [&](const auto &p) { return !is_rigid(p.first, p.second); });
      it != action.pre_instantiated.end()) {
    std::for_each(
        it, action.pre_instantiated.end(), [&](const auto &precondition) {
          assert(
              (precondition.second
                   ? pos_precondition_support_
                   : neg_precondition_support_)[get_id(precondition.first)] >=
              num_action_instantiations);
          (precondition.second
               ? pos_precondition_support_
               : neg_precondition_support_)[get_id(precondition.first)] -=
              num_action_instantiations;
        });
    action.pre_instantiated.erase(it, action.pre_instantiated.end());
    result = SimplifyResult::Changed;
  }

  return result;
  /* return SimplifyResult::Unchanged; */
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
    for (auto &action : partially_instantiated_actions_[i]) {
      preprocessed_problem->actions.push_back(std::move(action));
      preprocessed_problem->action_names.push_back(problem_->action_names[i]);
    }
  }

  return preprocessed_problem;
}
