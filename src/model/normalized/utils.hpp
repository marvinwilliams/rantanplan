#ifndef NORMALIZED_UTILS_HPP
#define NORMALIZED_UTILS_HPP

#include "model/normalized/model.hpp"
#include "util/combination_iterator.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <iterator>
#include <numeric>
#include <variant>
#include <vector>

namespace normalized {

inline bool is_subtype(TypeIndex subtype, TypeIndex type,
                       const Problem &problem) noexcept {
  if (subtype == type) {
    return true;
  }
  while (problem.types[subtype].supertype != subtype) {
    subtype = problem.types[subtype].supertype;
    if (subtype == type) {
      return true;
    }
  }
  return false;
}

struct ParameterSelection {
  std::vector<ParameterIndex> parameters;
  ActionIndex action;
};

struct ParameterMapping {
  ParameterSelection parameter_selection;
  std::vector<std::vector<ArgumentIndex>> arguments;
};

struct ParameterAssignment {
  std::vector<std::pair<ParameterIndex, ConstantIndex>> assignments;
  ActionIndex action;
};

inline PredicateInstantiation instantiate(const Condition &predicate) noexcept {
  std::vector<ConstantIndex> args;
  args.reserve(predicate.arguments.size());
  for (const auto &arg : predicate.arguments) {
    assert(arg.is_constant());
    args.push_back(arg.get_constant());
  }
  return PredicateInstantiation{predicate.definition, std::move(args)};
}

inline PredicateInstantiation instantiate(const Condition &predicate,
                                          const Action &action) noexcept {
  std::vector<ConstantIndex> args;
  args.reserve(predicate.arguments.size());
  for (const auto &arg : predicate.arguments) {
    if (arg.is_constant()) {
      args.push_back(arg.get_constant());
    } else {
      assert(action.get(arg.get_parameter()).is_constant());
      args.push_back(action.get(arg.get_parameter()).get_constant());
    }
  }
  return PredicateInstantiation{predicate.definition, std::move(args)};
}

// returns true if every argument is constant
inline bool update_arguments(Condition &condition, const Action &action) {
  bool is_grounded = true;
  for (auto &arg : condition.arguments) {
    if (!arg.is_constant()) {
      if (action.parameters[arg.get_parameter()].is_constant()) {
        arg.set(action.get(arg.get_parameter()).get_constant());
      } else {
        is_grounded = false;
      }
    }
  }
  return is_grounded;
}

inline Action ground(const ParameterAssignment &assignment,
                     const Problem &problem) noexcept {
  Action new_action;
  new_action.parameters = problem.get(assignment.action).parameters;
  for (auto [p, c] : assignment.assignments) {
    new_action.parameters[p].set(c);
  }
  new_action.pre_instantiated = problem.get(assignment.action).pre_instantiated;
  new_action.eff_instantiated = problem.get(assignment.action).eff_instantiated;

  for (auto pred : problem.get(assignment.action).preconditions) {
    if (bool is_grounded = update_arguments(pred, new_action); is_grounded) {
      new_action.pre_instantiated.emplace_back(instantiate(pred),
                                               pred.positive);
    } else {
      new_action.preconditions.push_back(std::move(pred));
    }
  }
  for (auto pred : problem.get(assignment.action).effects) {
    if (bool is_grounded = update_arguments(pred, new_action); is_grounded) {
      new_action.eff_instantiated.emplace_back(instantiate(pred),
                                               pred.positive);
    } else {
      new_action.effects.push_back(std::move(pred));
    }
  }
  return new_action;
}

inline ParameterMapping get_mapping(ActionIndex action,
                                    const Condition &predicate,
                                    const Problem &problem) noexcept {
  std::vector<std::vector<ArgumentIndex>> parameter_matches(
      problem.get(action).parameters.size());
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    if (!predicate.arguments[i].is_constant()) {
      parameter_matches[predicate.arguments[i].get_parameter()].emplace_back(i);
    }
  }

  ParameterMapping mapping;
  for (size_t i = 0; i < problem.get(action).parameters.size(); ++i) {
    if (!parameter_matches[i].empty()) {
      mapping.parameter_selection.parameters.emplace_back(i);
      mapping.arguments.push_back(std::move(parameter_matches[i]));
    }
  }
  mapping.parameter_selection.action = action;
  return mapping;
}

inline ParameterAssignment
get_assignment(const ParameterMapping &mapping,
               const std::vector<ConstantIndex> &arguments) noexcept {
  assert(mapping.parameters.size() == arguments.size());
  ParameterAssignment assignment;
  assignment.assignments.reserve(mapping.parameter_selection.parameters.size());
  for (size_t i = 0; i < mapping.parameter_selection.parameters.size(); ++i) {
    assignment.assignments.push_back(
        {mapping.parameter_selection.parameters[i], arguments[i]});
  }
  assignment.action = mapping.parameter_selection.action;
  return assignment;
}

inline size_t get_num_instantiated(const Predicate &predicate,
                                   const Problem &problem) {

  return std::accumulate(
      predicate.parameter_types.begin(), predicate.parameter_types.end(), 1ul,
      [&problem](size_t product, TypeIndex type) {
        return product * problem.constants_by_type[type].size();
      });
}

inline size_t get_num_instantiated(const Action &action,
                                   const Problem &problem) noexcept {
  return std::accumulate(
      action.parameters.begin(), action.parameters.end(), 1ul,
      [&problem](size_t product, const Parameter &p) {
        return p.is_constant()
                   ? product
                   : product * problem.constants_by_type[p.get_type()].size();
      });
}

inline size_t get_num_instantiated(const Condition &condition,
                                   const Action &action,
                                   const Problem &problem) noexcept {
  return std::accumulate(
      condition.arguments.begin(), condition.arguments.end(), 1ul,
      [&action, &problem](size_t product, const Argument &a) {
        return a.is_constant()
                   ? product
                   : product *
                         problem
                             .constants_by_type[action.get(a.get_parameter())
                                                    .get_type()]
                             .size();
      });
}

template <typename Function>
void for_each_instantiation(const Predicate &predicate, Function &&f,
                            const Problem &problem) noexcept {
  std::vector<size_t> number_arguments;
  number_arguments.reserve(predicate.parameter_types.size());
  for (auto type : predicate.parameter_types) {
    number_arguments.push_back(problem.constants_by_type[type].size());
  }

  auto combination_iterator = CombinationIterator{std::move(number_arguments)};

  std::vector<ConstantIndex> arguments(predicate.parameter_types.size());
  while (!combination_iterator.end()) {
    const auto &combination = *combination_iterator;
    for (size_t i = 0; i < combination.size(); ++i) {
      auto type = predicate.parameter_types[i];
      arguments[i] = problem.constants_by_type[type][combination[i]];
    }
    f(arguments);
    ++combination_iterator;
  }
}

template <typename Function>
void for_each_action_instantiation(const ParameterSelection &selection,
                                   Function &&f, const Problem &problem) {
  std::vector<size_t> argument_size_list;
  argument_size_list.reserve(selection.parameters.size());
  const auto &action = problem.get(selection.action);
  for (auto p : selection.parameters) {
    assert(!problem.get(action.get(p).is_constant()));
    argument_size_list.push_back(
        problem.constants_by_type[action.get(p).get_type()].size());
  }

  auto combination_iterator = CombinationIterator{argument_size_list};

  while (!combination_iterator.end()) {
    const auto &combination = *combination_iterator;
    ParameterAssignment assignment;
    assignment.assignments.reserve(combination.size());
    for (size_t i = 0; i < combination.size(); ++i) {
      assert(!action.get(selection.parameters[i]).is_constant());
      auto type = action.get(selection.parameters[i]).get_type();
      assignment.assignments.emplace_back(
          selection.parameters[i],
          problem.constants_by_type[type][combination[i]]);
    }
    assignment.action = selection.action;
    f(std::move(assignment));
    ++combination_iterator;
  }
}

/* inline bool is_unifiable(const std::vector<Parameter> &first, */
/*                          const std::vector<Parameter> &second) noexcept {
 */
/*   // Assumes same action, thus free parameters are not checked */
/*   assert(first.size() == second.size()); */
/*   for (size_t i = 0; i < first.size(); ++i) { */
/*     if (first[i].is_constant() && second[i].is_constant() && */
/*         first[i].get_constant() != second[i].get_constant()) { */
/*       return false; */
/*     } */
/*   } */
/*   return true; */
/* } */

inline bool is_instantiatable(const Condition &condition,
                              const std::vector<ConstantIndex> &instantiation,
                              const Action &action,
                              const Problem &problem) noexcept {
  auto parameters = action.parameters;
  for (size_t i = 0; i < condition.arguments.size(); ++i) {
    if (condition.arguments[i].is_constant()) {
      if (condition.arguments[i].get_constant() != instantiation[i]) {
        return false;
      }
    } else {
      auto &p = parameters[condition.arguments[i].get_parameter()];
      if (p.is_constant()) {
        if (p.get_constant() != instantiation[i]) {
          return false;
        }
      } else {
        if (!is_subtype(problem.get(instantiation[i]).type, p.get_type(),
                        problem)) {
          return false;
        }
        p.set(instantiation[i]);
      }
    }
  }
  return true;
}

} // namespace normalized

#endif /* end of include guard: NORMALIZED_UTILS_HPP */
