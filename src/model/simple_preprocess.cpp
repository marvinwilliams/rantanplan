#include "config.hpp"
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
      : init(problem.predicates.size()),
        constants_by_type(problem.types.size()),
        pos_rigid(problem.predicates.size()),
        neg_rigid(problem.predicates.size()),
        pos_tested(problem.predicates.size()),
        neg_tested(problem.predicates.size()), problem{problem} {

    trivially_rigid.reserve(problem.predicates.size());
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

  bool is_rigid(const PredicateInstantiation &predicate, bool positive) const {
    auto handle = get_handle(predicate, problem.constants.size());
    auto &rigid = (positive ? pos_rigid : neg_rigid)[predicate.definition];
    auto &tested = (positive ? pos_tested : neg_tested)[predicate.definition];

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

  template <typename Function> void refine(Function &&predicate_to_ground) {
    LOG_INFO(preprocess_logger, "New grounding iteration with %lu action(s)",
             std::accumulate(partially_instantiated_actions.begin(),
                             partially_instantiated_actions.end(), 0ul,
                             [](size_t sum, const auto &assignments) {
                               return sum + assignments.size();
                             }));

    refinement_possible = false;
    std::for_each(pos_tested.begin(), pos_tested.end(),
                  [](auto &t) { t.clear(); });
    std::for_each(neg_tested.begin(), neg_tested.end(),
                  [](auto &t) { t.clear(); });
    for (size_t i = 0; i < problem.actions.size(); ++i) {
      std::vector<Action> new_actions;
      for (const auto &action : partially_instantiated_actions[i]) {
        bool valid = true;
        for (const auto &[predicate, positive] : action.pre_instantiated) {
          if (is_rigid(predicate, !positive)) {
            valid = false;
            break;
          }
        }

        if (!valid) {
          refinement_possible = true;
          continue;
        }

        const Condition *to_ground = predicate_to_ground(action.preconditions);

        if (!to_ground) {
          new_actions.push_back(action);
          continue;
        }

        refinement_possible = true;

        for_each_assignment(
            *to_ground, action.parameters,
            [this, &action,
             &new_actions](const ParameterAssignment &assignment) {
              Action new_action = ground(action, assignment);
              bool valid = true;
              for (const auto &[predicate, positive] :
                   new_action.pre_instantiated) {
                if (is_rigid(predicate, !positive)) {
                  valid = false;
                  break;
                }
              }
              if (valid) {
                new_actions.push_back(std::move(new_action));
              }
            },
            constants_by_type);
      }
      partially_instantiated_actions[i] = std::move(new_actions);
    }
  }

  std::vector<std::unordered_set<InstantiationHandle>> init;
  std::vector<std::vector<ConstantHandle>> constants_by_type;

  bool refinement_possible = true;
  std::vector<std::vector<Action>> partially_instantiated_actions;
  std::vector<bool> trivially_rigid;
  mutable std::vector<std::unordered_set<InstantiationHandle>> pos_rigid;
  mutable std::vector<std::unordered_set<InstantiationHandle>> neg_rigid;
  mutable std::vector<std::unordered_set<InstantiationHandle>> pos_tested;
  mutable std::vector<std::unordered_set<InstantiationHandle>> neg_tested;
  const Problem &problem;
};

void preprocess(Problem &problem, const Config &config) {
  if (config.preprocess_mode == Config::PreprocessMode::None) {
    LOG_INFO(preprocess_logger, "No preprocessing done");
    return;
  }

  LOG_INFO(preprocess_logger, "Generating preprocessing support...");
  PreprocessSupport support{problem};

  auto select_min_grounded =
      [&support](const std::vector<Condition> &conditions) {
        size_t min_value = std::numeric_limits<size_t>::max();
        const Condition *min = nullptr;

        for (const auto &condition : conditions) {
          size_t value =
              get_num_grounded(condition.arguments, support.constants_by_type);
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
}
