#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "model/to_string.hpp"
#include "preprocess/preprocess.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <vector>

logging::Logger preprocess_logger = logging::Logger{"Preprocess"};

using namespace normalized;

PreprocessSupport::PreprocessSupport(const Problem &problem) noexcept
    : problem{problem} {

  num_actions =
      std::accumulate(problem.actions.begin(), problem.actions.end(), 0ul,
                      [&problem](uint_fast64_t sum, const auto &a) {
                        return sum + get_num_instantiated(a, problem);
                      });

  predicate_id_offset.reserve(problem.predicates.size());
  predicate_id_offset.push_back(0);
  for (auto it = problem.predicates.begin(); it != problem.predicates.end() - 1;
       ++it) {
    predicate_id_offset.push_back(
        predicate_id_offset.back() +
        static_cast<uint_fast64_t>(
            std::pow(problem.constants.size(), it->parameter_types.size())));
  }
  trivially_rigid.reserve(problem.predicates.size());
  trivially_effectless.reserve(problem.predicates.size());
  for (size_t i = 0; i < problem.predicates.size(); ++i) {
    trivially_rigid.push_back(std::none_of(
        problem.actions.begin(), problem.actions.end(),
        [index = PredicateIndex{i}](const auto &action) {
          return std::any_of(action.effects.begin(), action.effects.end(),
                             [index](const auto &effect) {
                               return effect.definition == index;
                             });
        }));
    trivially_effectless.push_back(std::none_of(
        problem.actions.begin(), problem.actions.end(),
        [index = PredicateIndex{i}](const auto &action) {
          return std::any_of(action.preconditions.begin(),
                             action.preconditions.end(),
                             [index](const auto &precondition) {
                               return precondition.definition == index;
                             });
        }));
  }

  partially_instantiated_actions.reserve(problem.actions.size());
  for (const auto &action : problem.actions) {
    partially_instantiated_actions.push_back({action});
  }

  init.reserve(problem.init.size());
  for (const auto &predicate : problem.init) {
    init.push_back(get_id(predicate));
  }
  std::sort(init.begin(), init.end());

  goal.reserve(problem.goal.size());
  for (const auto &[predicate, positive] : problem.goal) {
    goal.emplace_back(get_id(predicate), positive);
  }
  std::sort(goal.begin(), goal.end());
}

PreprocessSupport::PredicateId
PreprocessSupport::get_id(const PredicateInstantiation &predicate) const
    noexcept {
  uint_fast64_t result = 0;
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    result = (result * problem.constants.size()) + predicate.arguments[i];
  }
  result += predicate_id_offset[predicate.definition];
  return result;
}

bool PreprocessSupport::is_trivially_rigid(
    const PredicateInstantiation &predicate, bool positive) const noexcept {
  if (std::binary_search(init.begin(), init.end(), get_id(predicate)) !=
      positive) {
    return false;
  }
  return trivially_rigid[predicate.definition];
}

bool PreprocessSupport::is_trivially_effectless(
    const PredicateInstantiation &predicate, bool positive) const noexcept {
  if (std::binary_search(goal.begin(), goal.end(),
                         std::make_pair(get_id(predicate), positive))) {
    return false;
  }
  return trivially_effectless[predicate.definition];
}

bool PreprocessSupport::has_effect(const Action &action,
                                   const PredicateInstantiation &predicate,
                                   bool positive) const noexcept {
  for (const auto &[effect, eff_positive] : action.eff_instantiated) {
    if (eff_positive == positive && effect == predicate) {
      return true;
    }
  }
  for (const auto &effect : action.effects) {
    if (effect.definition == predicate.definition &&
        effect.positive == positive) {
      if (is_instantiatable(effect, predicate.arguments, action, problem)) {
        return true;
      }
    }
  }
  return false;
}

bool PreprocessSupport::has_precondition(
    const Action &action, const PredicateInstantiation &predicate,
    bool positive) const noexcept {
  for (const auto &[precondition, pre_positive] : action.pre_instantiated) {
    if (pre_positive == positive && precondition == predicate) {
      return true;
    }
  }
  for (const auto &precondition : action.preconditions) {
    if (precondition.definition == predicate.definition &&
        precondition.positive == positive) {
      if (is_instantiatable(precondition, predicate.arguments, action,
                            problem)) {
        return true;
      }
    }
  }
  return false;
}

// No action has this predicate as effect and it is not in init
bool PreprocessSupport::is_rigid(const PredicateInstantiation &predicate,
                                 bool positive) const noexcept {
  if (auto it = rigid.find(predicate);
      it != rigid.end() && it->second == positive) {
    return true;
  }

  auto id = get_id(predicate);

  if (auto it = rigid_tested.find(id);
      it != rigid_tested.end() && it->second == positive) {
    return false;
  }

  rigid_tested.insert({id, positive});
  if (std::binary_search(init.begin(), init.end(), id) != positive) {
    return false;
  }
  if (trivially_rigid[predicate.definition]) {
    rigid.insert({predicate, positive});
    return true;
  }
  for (size_t i = 0; i < problem.actions.size(); ++i) {
    const auto &base_action = problem.actions[i];
    if (!has_effect(base_action, predicate, !positive)) {
      continue;
    }
    for (const auto &action : partially_instantiated_actions[i]) {
      if (has_effect(action, predicate, !positive)) {
        return false;
      }
    }
  }
  rigid.insert({predicate, positive});
  return true;
}

// No action has this predicate as precondition and it is a not a goal
bool PreprocessSupport::is_effectless(const PredicateInstantiation &predicate,
                                      bool positive) const noexcept {
  if (auto it = effectless.find(predicate);
      it != effectless.end() && it->second == positive) {
    return true;
  }

  auto id = get_id(predicate);

  if (auto it = effectless_tested.find(id);
      it != effectless_tested.end() && it->second == positive) {
    return false;
  }

  effectless_tested.insert({id, positive});
  if (std::binary_search(goal.begin(), goal.end(),
                         std::make_pair(id, positive))) {
    return false;
  }
  if (trivially_effectless[predicate.definition]) {
    effectless.insert({predicate, positive});
    return true;
  }
  for (size_t i = 0; i < problem.actions.size(); ++i) {
    const auto &action = problem.actions[i];
    if (!has_precondition(action, predicate, positive)) {
      continue;
    }
    for (const auto &action : partially_instantiated_actions[i]) {
      if (has_precondition(action, predicate, positive)) {
        return false;
      }
    }
  }
  effectless.insert({predicate, positive});
  return true;
}

size_t PreprocessSupport::get_num_rigid(const Condition &condition,
                                        const Action &action,
                                        const Problem &problem) noexcept {
  return static_cast<size_t>(std::count_if(
      rigid.begin(), rigid.end(),
      [&condition, &action, &problem](const auto &r) {
        return r.first.definition == condition.definition &&
               r.second == condition.positive &&
               is_instantiatable(condition, r.first.arguments, action, problem);
      }));
}

PreprocessSupport::SimplifyResult
PreprocessSupport::simplify(Action &action) noexcept {
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

void preprocess(Problem &problem, const Config &config) {
  LOG_INFO(preprocess_logger, "Generating preprocessing support...");
  PreprocessSupport support{problem};

  auto select_free = [](const Action &action) {
    auto first_free =
        std::find_if(action.parameters.begin(), action.parameters.end(),
                     [](const auto &p) { return !p.is_constant(); });
    if (first_free == action.parameters.end()) {
      return ParameterSelection{};
    }
    return ParameterSelection{{first_free - action.parameters.begin()}};
  };

  auto select_min_new = [&problem, &select_free](const Action &action) {
    auto min = std::min_element(
        action.preconditions.begin(), action.preconditions.end(),
        [&action, &problem](const auto &c1, const auto &c2) {
          return get_num_instantiated(c1, action, problem) <
                 get_num_instantiated(c2, action, problem);
        });

    if (min == action.preconditions.end()) {
      return select_free(action);
    }

    return get_referenced_parameters(action, *min);
  };

  auto select_max_rigid = [&select_free, &support,
                           &problem](const Action &action) {
    auto max = std::max_element(
        action.preconditions.begin(), action.preconditions.end(),
        [&action, &support, &problem](const auto &c1, const auto &c2) {
          return support.get_num_rigid(c1, action, problem) <
                 support.get_num_rigid(c2, action, problem);
        });

    if (max == action.preconditions.end()) {
      return select_free(action);
    }

    return get_mapping(action, *max).parameter_selection;
  };

  size_t num_iteration = 0;
  while (
      support.refinement_possible &&
      (config.num_iterations == 0 || num_iteration++ < config.num_iterations)) {
    if (config.preprocess_priority == Config::PreprocessPriority::New) {
      support.refine(select_min_new);
    } else if (config.preprocess_priority ==
               Config::PreprocessPriority::Rigid) {
      support.refine(select_max_rigid);
    } else if (config.preprocess_priority == Config::PreprocessPriority::Free) {
      support.refine(select_free);
    }
  }

  size_t num_actions = problem.actions.size();
  problem.actions.clear();
  auto action_names = std::move(problem.action_names);
  problem.action_names.clear(); // noop

  for (size_t i = 0; i < num_actions; ++i) {
    for (auto &action : support.partially_instantiated_actions[i]) {
      problem.actions.push_back(std::move(action));
      problem.action_names.push_back(action_names[i]);
    }
  }
  LOG_DEBUG(preprocess_logger, "Preprocessed problem:\n%s",
            ::to_string(problem).c_str());
}
