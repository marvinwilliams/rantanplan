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
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#ifdef PARALLEL
#include <future>
#endif

logging::Logger preprocess_logger = logging::Logger{"Preprocess"};

using namespace normalized;

Preprocessor::Preprocessor(const Problem &problem) noexcept
    : problem_{problem} {

  LOG_INFO(preprocess_logger, "Initialize preprocessor");
  num_actions_ =
      std::accumulate(problem.actions.begin(), problem.actions.end(), 0ul,
                      [&problem](uint_fast64_t sum, const auto &a) {
                        return sum + get_num_instantiated(a, problem);
                      });

  predicate_id_offset_.reserve(problem.predicates.size());
  predicate_id_offset_.push_back(0);
  for (auto it = problem.predicates.begin(); it != problem.predicates.end() - 1;
       ++it) {
    predicate_id_offset_.push_back(
        predicate_id_offset_.back() +
        static_cast<uint_fast64_t>(
            std::pow(problem.constants.size(), it->parameter_types.size())));
  }
  trivially_rigid_.reserve(problem.predicates.size());
  trivially_effectless_.reserve(problem.predicates.size());
  for (size_t i = 0; i < problem.predicates.size(); ++i) {
    trivially_rigid_.push_back(std::none_of(
        problem.actions.begin(), problem.actions.end(),
        [index = PredicateIndex{i}](const auto &action) {
          return std::any_of(action.eff_instantiated.begin(),
                             action.eff_instantiated.end(),
                             [index](const auto &effect) {
                               return effect.first.definition == index;
                             }) ||
                 std::any_of(action.effects.begin(), action.effects.end(),
                             [index](const auto &effect) {
                               return effect.definition == index;
                             });
        }));
    trivially_effectless_.push_back(std::none_of(
        problem.actions.begin(), problem.actions.end(),
        [index = PredicateIndex{i}](const auto &action) {
          return std::any_of(action.pre_instantiated.begin(),
                             action.pre_instantiated.end(),
                             [index](const auto &precondition) {
                               return precondition.first.definition == index;
                             }) ||
                 std::any_of(action.preconditions.begin(),
                             action.preconditions.end(),
                             [index](const auto &precondition) {
                               return precondition.definition == index;
                             });
        }));
  }

  partially_instantiated_actions_.reserve(problem.actions.size());
  for (const auto &action : problem.actions) {
    partially_instantiated_actions_.push_back({action});
  }

  init_.reserve(problem.init.size());
  for (const auto &predicate : problem.init) {
    init_.push_back(get_id(predicate));
  }
  std::sort(init_.begin(), init_.end());

  goal_.reserve(problem.goal.size());
  for (const auto &[predicate, positive] : problem.goal) {
    goal_.emplace_back(get_id(predicate), positive);
  }
  std::sort(goal_.begin(), goal_.end());
}

Preprocessor::PredicateId
Preprocessor::get_id(const PredicateInstantiation &predicate) const noexcept {
  uint_fast64_t result = 0;
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    result = (result * problem_.constants.size()) + predicate.arguments[i];
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
      if (is_instantiatable(effect, predicate.arguments, action, problem_)) {
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
                            problem_)) {
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

  if (auto it = rigid_.find(id); it != rigid_.end() && it->second == positive) {
    return true;
  }

  if (std::binary_search(init_.begin(), init_.end(), id) != positive) {
    return false;
  }

  if (trivially_rigid_[predicate.definition]) {
    return true;
  }

  for (size_t i = 0; i < problem_.actions.size(); ++i) {
    const auto &base_action = problem_.actions[i];
    if (!has_effect(base_action, predicate, !positive)) {
      continue;
    }
    for (const auto &action : partially_instantiated_actions_[i]) {
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

  if (auto it = effectless_.find(id);
      it != effectless_.end() && it->second == positive) {
    return true;
  }

  if (std::binary_search(goal_.begin(), goal_.end(),
                         std::make_pair(id, positive))) {
    return false;
  }
  if (trivially_effectless_[predicate.definition]) {
    return true;
  }
  for (size_t i = 0; i < problem_.actions.size(); ++i) {
    const auto &action = problem_.actions[i];
    if (!has_precondition(action, predicate, positive)) {
      continue;
    }
    for (const auto &action : partially_instantiated_actions_[i]) {
      if (has_precondition(action, predicate, positive)) {
        return false;
      }
    }
  }
  return true;
}

Preprocessor::SimplifyResult Preprocessor::simplify(Action &action) noexcept {
  if (std::any_of(
          action.pre_instantiated.begin(), action.pre_instantiated.end(),
          [this](const auto &p) { return is_rigid(p.first, !p.second); })) {
    return SimplifyResult::Invalid;
  }

  bool changed = false;

  if (auto it = std::remove_if(
          action.eff_instantiated.begin(), action.eff_instantiated.end(),
          [this](const auto &p) { return is_rigid(p.first, p.second); });
      it != action.eff_instantiated.end()) {
    action.eff_instantiated.erase(it, action.eff_instantiated.end());
    changed = true;
  }

  if (action.effects.empty() &&
      std::all_of(
          action.eff_instantiated.begin(), action.eff_instantiated.end(),
          [this](const auto &p) { return is_effectless(p.first, p.second); })) {
    return SimplifyResult::Invalid;
  }

  if (auto it = std::remove_if(
          action.pre_instantiated.begin(), action.pre_instantiated.end(),
          [this](const auto &p) { return is_rigid(p.first, p.second); });
      it != action.pre_instantiated.end()) {
    action.pre_instantiated.erase(it, action.pre_instantiated.end());
    changed = true;
  }

  return changed ? SimplifyResult::Changed : SimplifyResult::Unchanged;
}

#ifdef PARALLEL
std::pair<Problem, Planner::Plan>
Preprocessor::preprocess(const Planner &planner,
                         const Config &config) noexcept {
#else
Problem Preprocessor::preprocess(const Config &config) noexcept {
#endif

  auto select_free = [](const Action &action) {
    auto first_free =
        std::find_if(action.parameters.begin(), action.parameters.end(),
                     [](const auto &p) { return !p.is_constant(); });
    if (first_free == action.parameters.end()) {
      return ParameterSelection{};
    }
    return ParameterSelection{{first_free - action.parameters.begin()}};
  };

  auto select_min_new = [&select_free, this](const Action &action) {
    auto min = std::min_element(
        action.preconditions.begin(), action.preconditions.end(),
        [&action, this](const auto &c1, const auto &c2) {
          return get_num_instantiated(c1, action, problem_) <
                 get_num_instantiated(c2, action, problem_);
        });

    if (min == action.preconditions.end()) {
      return select_free(action);
    }

    return get_referenced_parameters(action, *min);
  };

  auto select_max_rigid = [&select_free, this](const Action &action) {
    auto max = std::max_element(
        action.preconditions.begin(), action.preconditions.end(),
        [this](const auto &c1, const auto &c2) {
          return std::count_if(rigid_.begin(), rigid_.end(),
                               [&c1](const auto &r) {
                                 return r.first == c1.definition &&
                                        r.second != c1.positive;
                               }) <
                 std::count_if(
                     rigid_.begin(), rigid_.end(),
                     [&c2](const auto &r) {
                       return r.first == c2.definition &&
                              r.second != c2.positive;
                     });
        });

    if (max == action.preconditions.end()) {
      return select_free(action);
    }

    return get_mapping(action, *max).parameters;
  };
  auto call_refine = [&]() {
    switch (config.preprocess_priority) {
    case Config::PreprocessPriority::New:
      return refine(select_min_new);
    case Config::PreprocessPriority::Rigid:
      return refine(select_max_rigid);
    case Config::PreprocessPriority::Free:
      return refine(select_free);
    }
    return refine(select_free);
  };

#ifdef PARALLEL
  std::atomic_bool plan_found = false;
  threads_.clear();
  if (config.num_threads <= 1) {
    if (config.num_threads == 1) {
      while (refinement_possible_) {
        call_refine();
      }
    }
    auto problem = to_problem();
    auto plan = planner.plan(problem, config, plan_found);
    return {std::move(problem), std::move(plan)};
  } else {
    std::promise<std::pair<Problem, Planner::Plan>> plan_promise;
    auto plan_future = plan_promise.get_future();
    float progress_steps = 1.0f / (static_cast<float>(config.num_threads) - 1);
    float progress = 0.0f;
    num_free_threads_ = config.num_threads;
    PRINT_INFO("Start planner with progress %f", progress);
    threads_.emplace_back([&, problem = to_problem()]() {
      auto plan = planner.plan(problem, config, plan_found);
      if (!plan.empty()) {
        plan_promise.set_value(
            std::make_pair(std::move(problem), std::move(plan)));
      }
    });
    --num_free_threads_;
    float next_progress = progress_steps;
    while (refinement_possible_ && !plan_found) {
      while (refinement_possible_ && !plan_found &&
             (progress < next_progress || progress == 1.0f)) {
        progress = call_refine();
      }
      if (plan_found) {
        break;
      }
      PRINT_INFO("Start planner with progress %f", progress);
      threads_.emplace_back([&, problem = to_problem()]() {
        auto plan = planner.plan(problem, config, plan_found);
        if (!plan.empty()) {
          plan_promise.set_value(
              std::make_pair(std::move(problem), std::move(plan)));
        }
      });
      --num_free_threads_;
      do {
        next_progress += progress_steps;
      } while (next_progress <= progress);
    }
    return plan_future.get();
  }
#else
  float progress = 0.0f;
  while (refinement_possible_ && progress <= config.preprocess_progress) {
    progress = call_refine();
  }
  return to_problem();
#endif
}

Problem Preprocessor::to_problem() const noexcept {
  Problem preprocessed_problem{};
  preprocessed_problem.domain_name = problem_.domain_name;
  preprocessed_problem.problem_name = problem_.problem_name;
  preprocessed_problem.requirements = problem_.requirements;
  preprocessed_problem.types = problem_.types;
  preprocessed_problem.type_names = problem_.type_names;
  preprocessed_problem.constants = problem_.constants;
  preprocessed_problem.constant_names = problem_.constant_names;
  preprocessed_problem.constants_by_type = problem_.constants_by_type;
  preprocessed_problem.predicates = problem_.predicates;
  preprocessed_problem.predicate_names = problem_.predicate_names;
  preprocessed_problem.init = problem_.init;
  std::copy_if(problem_.goal.begin(), problem_.goal.end(),
               std::back_inserter(preprocessed_problem.goal),
               [this](const auto &g) { return !is_rigid(g.first, g.second); });

  for (size_t i = 0; i < problem_.actions.size(); ++i) {
    for (auto &action : partially_instantiated_actions_[i]) {
      preprocessed_problem.actions.push_back(std::move(action));
      preprocessed_problem.action_names.push_back(problem_.action_names[i]);
    }
  }

  LOG_DEBUG(preprocess_logger, "Preprocessed problem:\n%s",
            ::to_string(preprocessed_problem).c_str());

  return preprocessed_problem;
}
