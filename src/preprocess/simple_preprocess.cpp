#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "model/to_string.hpp"
#include "preprocess/preprocess.hpp"

#include <limits>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <vector>

logging::Logger preprocess_logger = logging::Logger{"Preprocess"};

using namespace normalized;

PreprocessSupport::PreprocessSupport(const Problem &problem) noexcept
    : problem{problem} {

  num_actions = std::accumulate(
      problem.actions.begin(), problem.actions.end(), 0ul,
      [&problem](const auto &a) { return get_num_instantiated(a, problem); });

  predicate_id_offset.reserve(problem.predicates.size());
  predicate_id_offset.push_back(0);
  std::partial_sum(
      problem.predicates.begin(), problem.predicates.end() - 1,
      std::back_inserter(predicate_id_offset),
      [&problem](const auto &p) { return get_num_instantiated(p, problem); });

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

PredicateId
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
  auto id = get_id(predicate);

  if (auto it = rigid.find(id);
      it != rigid.end() && it->second == positive) {
    return true;
  }
  if (auto it = rigid_tested.find(id);
      it != rigid_tested.end() && it->second == positive) {
    return false;
  }

  rigid_tested.insert({id, positive});
  if (std::binary_search(init.begin(), init.end(), id) != positive) {
    return false;
  }
  if (trivially_rigid[get_index(predicate.definition, problem)]) {
    rigid.insert(id);
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
  rigid.insert(id);
  return true;
}

// No action has this predicate as precondition and it is a not a goal
bool is_effectless(const PredicateInstantiation &predicate,
                   bool positive) const {
  auto handle = get_id(predicate);
  auto &effectless = (positive ? pos_effectless : neg_effectless);
  auto &tested = (positive ? pos_effectless_tested : neg_effectless_tested);

  if (effectless.find(handle) != effectless.end()) {
    return true;
  }
  if (tested.find(handle) != tested.end()) {
    return false;
  }

  tested.insert(handle);
  if (auto it = goal.find(handle); it != goal.end() && it->second == positive) {
    return false;
  }
  if (trivially_effectless[get_index(predicate.definition, problem)]) {
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

template <typename Function> void refine(Function &&select_parameters) {
  LOG_INFO(preprocess_logger, "New grounding iteration with %lu action(s)",
           std::accumulate(partially_instantiated_actions.begin(),
                           partially_instantiated_actions.end(), 0ul,
                           [](size_t sum, const auto &assignments) {
                             return sum + assignments.size();
                           }));

  refinement_possible = false;
  reset_tested();
  /* size_t num_parameters = 0; */
  /* size_t num_constant_parameters = 0; */
  for (size_t i = 0; i < problem.actions.size(); ++i) {
    std::vector<Action> new_actions;
    for (auto &action : partially_instantiated_actions[i]) {
      if (auto result = simplify(action); result != SimplifyResult::Unchanged) {
        refinement_possible = true;
        if (result == SimplifyResult::Invalid) {
          continue;
        }
      }

      auto selection = select_parameters(action);

      if (selection.parameters.empty()) {
        new_actions.push_back(action);
        continue;
      }

      refinement_possible = true;

      for_each_action_instantiation(
          selection,
          [&action,
           &new_actions, /* &num_parameters, &num_constant_parameters,*/
           this](const ParameterAssignment &assignment) {
            auto new_action = ground(action, assignment);
            if (auto result = simplify(new_action);
                result != SimplifyResult::Invalid) {
              /* num_parameters += new_action.parameters.size(); */
              /* num_constant_parameters +=
               * static_cast<size_t>(std::count_if( */
              /* new_action.parameters.begin(), new_action.parameters.end(),
               */
              /* [](const auto &p) { return p.is_constant(); })); */
              new_actions.push_back(std::move(new_action));
            }
          },
          problem);
    }
    partially_instantiated_actions[i] = std::move(new_actions);
  }
  /* printf("Ratio: %f\n", */
  /*        static_cast<float>(num_constant_parameters) / num_parameters); */
}

bool refinement_possible = true;
unsigned num_constants_exponent;
std::vector<uint_fast64_t> handle_offset;
std::unordered_set<InstantiationHandle> init;
std::unordered_map<InstantiationHandle, bool> goal;
std::vector<std::vector<Action>> partially_instantiated_actions;
std::vector<bool> trivially_rigid;
std::vector<bool> trivially_effectless;
mutable std::unordered_set<InstantiationHandle> pos_rigid;
mutable std::unordered_set<InstantiationHandle> neg_rigid;
mutable std::unordered_set<InstantiationHandle> pos_rigid_tested;
mutable std::unordered_set<InstantiationHandle> neg_rigid_tested;
mutable std::unordered_set<InstantiationHandle> pos_effectless;
mutable std::unordered_set<InstantiationHandle> neg_effectless;
mutable std::unordered_set<InstantiationHandle> pos_effectless_tested;
mutable std::unordered_set<InstantiationHandle> neg_effectless_tested;
const Problem &problem;
}
;

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
    return ParameterSelection{{&*first_free}, &action};
  };

  auto select_min_new = [&problem, &select_free](const Action &action) {
    auto min = std::min_element(action.preconditions.begin(),
                                action.preconditions.end(),
                                [&problem](const auto &c1, const auto &c2) {
                                  return get_num_instantiated(c1, problem) <
                                         get_num_instantiated(c2, problem);
                                });
    if (min == action.preconditions.end()) {
      return select_free(action);
    }
    return get_mapping(action, *min).parameter_selection;
  };

  auto select_max_rigid = [&support, &select_free](const Action &action) {
    auto max = std::max_element(
        action.preconditions.begin(), action.preconditions.end(),
        [&support](const auto &c1, const auto &c2) {
          return (c1.positive ? support.neg_rigid : support.pos_rigid).size() >
                 (c2.positive ? support.neg_rigid : support.pos_rigid).size();
        });
    if (max == action.preconditions.end()) {
      return select_free(action);
    }
    return get_mapping(action, *max).parameter_selection;
  };

  size_t num_iteration = 0;
  while (support.refinement_possible &&
         num_iteration++ < config.num_iterations) {
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
