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
    for (const auto &[p, positive] : problem.goal) {
      assert(goal[p.definition].find(get_handle(p, problem.constants.size())) ==
             goal[p.definition].end());
      goal[p.definition].try_emplace(get_handle(p, problem.constants.size()),
                                     positive);
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
    if (auto it = goal[predicate.definition].find(handle);
        it != goal[predicate.definition].end() && it->second == positive) {
      return false;
    }
    return trivially_effectless[predicate.definition];
  }

  bool has_effect(const Action &action, const PredicateInstantiation &predicate,
                  bool positive) const {
    for (const auto &[effect, eff_positive] : action.eff_instantiated) {
      if (eff_positive == positive && effect == predicate) {
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
      if (pre_positive == positive && precondition == predicate) {
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
      if (!has_precondition(action, predicate, positive)) {
        continue;
      }
      for (const auto &action : partially_instantiated_actions[i]) {
        if (has_precondition(action, predicate, positive)) {
          return false;
        }
      }
    }
    effectless.insert(handle);
    return true;
  }

  void reset_tested() {
    std::for_each(pos_rigid_tested.begin(), pos_rigid_tested.end(),
                  [](auto &t) { t.clear(); });
    std::for_each(neg_rigid_tested.begin(), neg_rigid_tested.end(),
                  [](auto &t) { t.clear(); });
    std::for_each(pos_effectless_tested.begin(), pos_effectless_tested.end(),
                  [](auto &t) { t.clear(); });
    std::for_each(neg_effectless_tested.begin(), neg_effectless_tested.end(),
                  [](auto &t) { t.clear(); });
  }

  enum class SimplifyResult { Unchanged, Changed, Invalid };

  SimplifyResult simplify(Action &action) {
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
        std::all_of(action.eff_instantiated.begin(),
                    action.eff_instantiated.end(), [this](const auto &p) {
                      return is_effectless(p.first, p.second);
                    })) {
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

  template <typename Function> void refine(Function &&select_parameters) {
    LOG_INFO(preprocess_logger, "New grounding iteration with %lu action(s)",
             std::accumulate(partially_instantiated_actions.begin(),
                             partially_instantiated_actions.end(), 0ul,
                             [](size_t sum, const auto &assignments) {
                               return sum + assignments.size();
                             }));

    refinement_possible = false;
    reset_tested();
    size_t num_parameters = 0;
    size_t num_constant_parameters = 0;
    for (size_t i = 0; i < problem.actions.size(); ++i) {
      std::vector<Action> new_actions;
      for (auto &action : partially_instantiated_actions[i]) {
        if (auto result = simplify(action);
            result != SimplifyResult::Unchanged) {
          refinement_possible = true;
          if (result == SimplifyResult::Invalid) {
            continue;
          }
        }

        auto to_ground = select_parameters(action);

        if (to_ground.empty()) {
          new_actions.push_back(action);
          continue;
        }

        refinement_possible = true;

        for_each_action_instantiation(
            action.parameters, to_ground,
            [&action, &new_actions, &num_parameters, &num_constant_parameters,
             this](const ParameterAssignment &assignment) {
              auto new_action = ground(action, assignment);
              if (auto result = simplify(new_action);
                  result != SimplifyResult::Invalid) {
                num_parameters += new_action.parameters.size();
                num_constant_parameters += static_cast<size_t>(std::count_if(
                    new_action.parameters.begin(), new_action.parameters.end(),
                    [](const auto &p) { return p.is_constant(); }));
                new_actions.push_back(std::move(new_action));
              }
            },
            constants_by_type);
      }
      partially_instantiated_actions[i] = std::move(new_actions);
    }
    printf("Ratio: %f\n",
           static_cast<float>(num_constant_parameters) / num_parameters);
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

void preprocess(Problem &problem, const Config &config) {
  LOG_INFO(preprocess_logger, "Generating preprocessing support...");
  PreprocessSupport support{problem};

  auto select_free = [](const Action &action) {
    auto first_free =
        std::find_if(action.parameters.begin(), action.parameters.end(),
                     [](const auto &p) { return !p.is_constant(); });
    if (first_free == action.parameters.end()) {
      return std::vector<ParameterHandle>{};
    }
    return std::vector<ParameterHandle>{ParameterHandle{static_cast<size_t>(
        std::distance(action.parameters.begin(), first_free))}};
  };

  auto select_min_new = [&support, &select_free](const Action &action) {
    auto min = std::min_element(
        action.preconditions.begin(), action.preconditions.end(),
        [&support, &action](const auto &c1, const auto &c2) {
          return get_num_grounded(c1.arguments, action.parameters,
                                  support.constants_by_type) <
                 get_num_grounded(c2.arguments, action.parameters,
                                  support.constants_by_type);
        });
    if (min == action.preconditions.end()) {
      return select_free(action);
    }
    return get_mapping(action.parameters, *min).parameters;
  };

  auto select_max_rigid = [&support, &select_free](const Action &action) {
    auto max = std::max_element(
        action.preconditions.begin(), action.preconditions.end(),
        [&support](const auto &c1, const auto &c2) {
          return (c1.positive ? support.neg_rigid
                              : support.pos_rigid)[c1.definition]
                     .size() > (c2.positive ? support.neg_rigid
                                            : support.pos_rigid)[c2.definition]
                                   .size();
        });
    if (max == action.preconditions.end()) {
      return select_free(action);
    }
    return get_mapping(action.parameters, *max).parameters;
  };

  while (support.refinement_possible) {
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
