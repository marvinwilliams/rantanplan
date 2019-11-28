#include "logging/logging.hpp"
#include "model/model.hpp"
#include "model/preprocess.hpp"
#include "model/to_string.hpp"
#include "model/utils.hpp"

#include <limits>
#include <numeric>
#include <unordered_map>
#include <unordered_set>

logging::Logger preprocess_logger = logging::Logger{"Preprocess"};

using namespace normalized;

struct PreprocessSupport {
  explicit PreprocessSupport(const Problem &problem) noexcept
      : init(problem.predicates.size()), goal(problem.predicates.size()),
        constants_by_type(problem.types.size()),
        pos_rigid(problem.predicates.size()),
        neg_rigid(problem.predicates.size()),
        pos_rigid_tested(problem.predicates.size()),
        neg_rigid_tested(problem.predicates.size()),
        pos_effectless(problem.predicates.size()),
        neg_effectless(problem.predicates.size()),
        pos_effectless_tested(problem.predicates.size()),
        neg_effectless_tested(problem.predicates.size()), problem{problem} {

    trivially_rigid.reserve(problem.predicates.size());
    trivially_effectless.reserve(problem.predicates.size());
    for (size_t i = 0; i < problem.predicates.size(); ++i) {
      trivially_rigid.push_back(std::none_of(
          problem.actions.begin(), problem.actions.end(),
          [i](const auto &action) {
            return std::any_of(action.effects.begin(), action.effects.end(),
                               [i](const auto &predicate) {
                                 return predicate.definition ==
                                        PredicateHandle{i};
                               });
          }));
      trivially_effectless.push_back(
          std::none_of(problem.actions.begin(), problem.actions.end(),
                       [i](const auto &action) {
                         return std::any_of(action.preconditions.begin(),
                                            action.preconditions.end(),
                                            [i](const auto &predicate) {
                                              return predicate.definition ==
                                                     PredicateHandle{i};
                                            });
                       }));
    }

    partially_instantiated_actions.reserve(problem.actions.size());
    for (const auto &action : problem.actions) {
      partially_instantiated_actions.push_back({action});
    }

    for (const auto &p : problem.init) {
      init[p.definition].insert(get_handle(p, problem.constants.size()));
    }

    for (size_t i = 0; i < problem.constants.size(); ++i) {
      const auto &c = problem.constants[i];
      auto t = c.type;
      constants_by_type[c.type].push_back(ConstantHandle{i});
      while (problem.types[t].parent != t) {
        t = problem.types[t].parent;
        constants_by_type[t].push_back(ConstantHandle{i});
      }
    }
  }

  bool is_trivially_rigid(const PredicateInstantiation &predicate,
                          bool positive) const {
    auto handle = get_handle(predicate, problem.constants.size());
    bool in_init = init[predicate.definition].find(handle) !=
                   init[predicate.definition].end();
    if (in_init != positive) {
      return false;
    }
    return trivially_rigid[predicate.definition];
  }

  bool is_trivially_effectless(const PredicateInstantiation &predicate,
                               bool positive) const {
    auto handle = get_handle(predicate, problem.constants.size());
    if (auto in_goal = goal[predicate.definition].find(handle);
        in_goal != goal[predicate.definition].end() &&
        in_goal->second == positive) {
      return false;
    }
    return trivially_effectless[predicate.definition];
  }

  bool has_effect(const Action &action, const PredicateInstantiation &predicate,
                  bool positive) const {
    for (const auto &[effect, eff_positive] : action.eff_instantiated) {
      if (eff_positive == positive &&
          effect.definition == predicate.definition &&
          get_handle(effect, problem.constants.size()) ==
              get_handle(predicate, problem.constants.size())) {
        return true;
      }
    }
    for (const auto &effect : action.effects) {
      if (effect.definition == predicate.definition &&
          effect.positive == positive) {
        if (is_instantiatable(effect, predicate.arguments, action.parameters,
                              problem.constants, problem.types)) {
          return true;
        }
      }
    }
    return false;
  }

  bool has_precondition(const Action &action,
                        const PredicateInstantiation &predicate,
                        bool positive) const {
    for (const auto &[precondition, pre_positive] : action.pre_instantiated) {
      if (pre_positive == positive &&
          precondition.definition == predicate.definition &&
          get_handle(precondition, problem.constants.size()) ==
              get_handle(predicate, problem.constants.size())) {
        return true;
      }
    }
    for (const auto &precondition : action.preconditions) {
      if (precondition.definition == predicate.definition &&
          precondition.positive == positive) {
        if (is_instantiatable(precondition, predicate.arguments,
                              action.parameters, problem.constants,
                              problem.types)) {
          return true;
        }
      }
    }
    return false;
  }

  // No action has this predicate as effect and it is not in init
  bool is_rigid(const PredicateInstantiation &predicate, bool positive) const {
    auto handle = get_handle(predicate, problem.constants.size());
    auto &rigid = (positive ? pos_rigid : neg_rigid)[predicate.definition];
    auto &tested =
        (positive ? pos_rigid_tested : neg_rigid_tested)[predicate.definition];

    if (rigid.find(handle) != rigid.end()) {
      return true;
    }
    if (tested.find(handle) != tested.end()) {
      return false;
    }

    tested.insert(handle);
    bool is_init = init[predicate.definition].find(handle) !=
                   init[predicate.definition].end();
    if (is_init != positive) {
      return false;
    }
    if (trivially_rigid[predicate.definition]) {
      rigid.insert(handle);
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
    rigid.insert(handle);
    return true;
  }

  // No action has this predicate as precondition and it is a not a goal
  bool is_effectless(const PredicateInstantiation &predicate,
                     bool positive) const {
    auto handle = get_handle(predicate, problem.constants.size());
    auto &effectless =
        (positive ? pos_effectless : neg_effectless)[predicate.definition];
    auto &tested = (positive ? pos_effectless_tested
                             : neg_effectless_tested)[predicate.definition];

    if (effectless.find(handle) != effectless.end()) {
      return true;
    }
    if (tested.find(handle) != tested.end()) {
      return false;
    }

    tested.insert(handle);
    if (auto it = goal[predicate.definition].find(handle);
        it != goal[predicate.definition].end() && it->second == positive) {
      return false;
    }
    if (trivially_effectless[predicate.definition]) {
      effectless.insert(handle);
      return true;
    }
    for (size_t i = 0; i < problem.actions.size(); ++i) {
      const auto &action = problem.actions[i];
      if (!has_precondition(action, predicate, !positive)) {
        continue;
      }
      for (const auto &action : partially_instantiated_actions[i]) {
        if (has_precondition(action, predicate, !positive)) {
          return false;
        }
      }
    }
    effectless.insert(handle);
    return true;
  }

  template <typename Function> void refine(Function &&predicate_to_ground) {
    LOG_INFO(preprocess_logger, "New grounding iteration with %lu action(s)",
             std::accumulate(partially_instantiated_actions.begin(),
                             partially_instantiated_actions.end(), 0ul,
                             [](size_t sum, const auto &assignments) {
                               return sum + assignments.size();
                             }));

    refinement_possible = false;
    std::for_each(pos_rigid_tested.begin(), pos_rigid_tested.end(),
                  [](auto &t) { t.clear(); });
    std::for_each(neg_rigid_tested.begin(), neg_rigid_tested.end(),
                  [](auto &t) { t.clear(); });
    std::for_each(pos_effectless_tested.begin(), pos_effectless_tested.end(),
                  [](auto &t) { t.clear(); });
    std::for_each(neg_effectless_tested.begin(), neg_effectless_tested.end(),
                  [](auto &t) { t.clear(); });
    for (size_t i = 0; i < problem.actions.size(); ++i) {
      std::vector<Action> new_actions;
      for (auto &action : partially_instantiated_actions[i]) {
        if (std::any_of(action.pre_instantiated.begin(),
                        action.pre_instantiated.end(), [this](const auto &p) {
                          return is_rigid(p.first, !p.second);
                        })) {
          refinement_possible = true;
          continue;
        }
        if (auto it = std::remove_if(
                action.pre_instantiated.begin(), action.pre_instantiated.end(),
                [this](const auto &p) { return is_rigid(p.first, p.second); });
            it != action.pre_instantiated.end()) {
          refinement_possible = true;
          action.pre_instantiated.erase(it, action.pre_instantiated.end());
        }
        if (auto it = std::remove_if(
                action.eff_instantiated.begin(), action.eff_instantiated.end(),
                [this](const auto &p) {
                  return is_effectless(p.first, p.second) ||
                         is_rigid(p.first, p.second);
                });
            it != action.eff_instantiated.end()) {
          refinement_possible = true;
          action.eff_instantiated.erase(it, action.eff_instantiated.end());
        }
        if (action.eff_instantiated.empty() && action.effects.empty()) {
          refinement_possible = true;
          continue;
        }

        const Condition *to_ground =
            predicate_to_ground(action.preconditions, action.parameters);

        if (!to_ground) {
          new_actions.push_back(action);
          continue;
        }

        refinement_possible = true;

        for_each_assignment(
            *to_ground, action.parameters,
            [&action, &new_actions](const ParameterAssignment &assignment) {
              new_actions.push_back(ground(action, assignment));
            },
            constants_by_type);
      }
      partially_instantiated_actions[i] = std::move(new_actions);
    }
  }

  std::vector<std::unordered_set<InstantiationHandle>> init;
  std::vector<std::unordered_map<InstantiationHandle, bool>> goal;
  std::vector<std::vector<ConstantHandle>> constants_by_type;

  bool refinement_possible = true;
  std::vector<std::vector<Action>> partially_instantiated_actions;
  std::vector<bool> trivially_rigid;
  std::vector<bool> trivially_effectless;
  mutable std::vector<std::unordered_set<InstantiationHandle>> pos_rigid;
  mutable std::vector<std::unordered_set<InstantiationHandle>> neg_rigid;
  mutable std::vector<std::unordered_set<InstantiationHandle>> pos_rigid_tested;
  mutable std::vector<std::unordered_set<InstantiationHandle>> neg_rigid_tested;
  mutable std::vector<std::unordered_set<InstantiationHandle>> pos_effectless;
  mutable std::vector<std::unordered_set<InstantiationHandle>> neg_effectless;
  mutable std::vector<std::unordered_set<InstantiationHandle>>
      pos_effectless_tested;
  mutable std::vector<std::unordered_set<InstantiationHandle>>
      neg_effectless_tested;
  const Problem &problem;
};

void preprocess(Problem &problem) {
  LOG_INFO(preprocess_logger, "Generating preprocessing support...");
  PreprocessSupport support{problem};

  auto select_min_grounded =
      [&support](const std::vector<Condition> &conditions,
                 const std::vector<Parameter> &parameters) {
        size_t min_value = std::numeric_limits<size_t>::max();
        const Condition *min = nullptr;

        for (const auto &condition : conditions) {
          size_t value = get_num_grounded(condition.arguments, parameters,
                                          support.constants_by_type);
          if (value < min_value) {
            min_value = value;
            min = &condition;
          }
        }
        return min;
      };

  while (support.refinement_possible) {
    support.refine(select_min_grounded);
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
