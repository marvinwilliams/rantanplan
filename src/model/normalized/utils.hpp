#ifndef MODEL_UTILS_HPP
#define MODEL_UTILS_HPP

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
  std::vector<const Parameter *> parameters;
  const Action *action;
};

struct ParameterMapping {
  ParameterSelection parameter_selection;
  std::vector<std::vector<const Argument *>> arguments;
  const Condition *condition;
};

struct ParameterAssignment {
  std::vector<std::pair<const Parameter *, const Constant *>> assignments;
  const Action *action;
};

template <typename T>
inline size_t get_index(const T *t, const std::vector<T> &list) noexcept {
  return static_cast<size_t>(std::distance(&list.front(), t));
}

inline size_t get_index(const Type *type, const Problem &problem) noexcept {
  return get_index(type, problem.types);
}
inline size_t get_index(const Constant *constant,
                        const Problem &problem) noexcept {
  return get_index(constant, problem.constants);
}
inline size_t get_index(const Predicate *predicate,
                        const Problem &problem) noexcept {
  return get_index(predicate, problem.predicates);
}
inline size_t get_index(const Action *action, const Problem &problem) noexcept {
  return get_index(action, problem.actions);
}

inline PredicateInstantiation instantiate(const Condition &predicate) noexcept {
  std::vector<const Constant *> args;
  args.reserve(predicate.arguments.size());
  for (const auto &arg : predicate.arguments) {
    if (arg.is_constant()) {
      args.push_back(&arg.get_constant());
    } else {
      assert(parameters[arg.get_parameter()].is_constant());
      args.push_back(&arg.get_parameter().get_constant());
    }
  }
  return PredicateInstantiation{*predicate.definition, std::move(args)};
}

inline Condition ground(const Condition &predicate) {
  Condition new_predicate;
  new_predicate.definition = predicate.definition;
  new_predicate.positive = predicate.positive;

  for (const auto &arg : predicate.arguments) {
    if (arg.is_constant()) {
      new_predicate.arguments.emplace_back(arg.get_constant());
    } else if (arg.get_parameter().is_constant()) {
      new_predicate.arguments.emplace_back(arg.get_parameter().get_constant());
    } else {
      new_predicate.arguments.emplace_back(arg.get_parameter());
    }
  }
  return new_predicate;
}

inline bool is_grounded(const Condition &predicate) noexcept {
  return std::all_of(predicate.arguments.cbegin(), predicate.arguments.cend(),
                     [](const auto &arg) { return arg.is_constant(); });
}

inline Action ground(const ParameterAssignment &assignment) noexcept {
  Action new_action;
  new_action.parameters = assignment.action->parameters;
  for (auto [p, c] : assignment.assignments) {
    new_action.parameters[get_index(p, assignment.action->parameters)].set(*c);
  }
  new_action.pre_instantiated = assignment.action->pre_instantiated;
  new_action.eff_instantiated = assignment.action->eff_instantiated;
  for (const auto &pred : assignment.action->preconditions) {
    if (auto new_pred = ground(pred); is_grounded(new_pred)) {
      new_action.pre_instantiated.emplace_back(instantiate(new_pred),
                                               pred.positive);
    } else {
      new_action.preconditions.push_back(std::move(new_pred));
    }
  }
  for (const auto &pred : assignment.action->effects) {
    if (auto new_pred = ground(pred); is_grounded(new_pred)) {
      new_action.eff_instantiated.emplace_back(instantiate(new_pred),
                                               pred.positive);
    } else {
      new_action.effects.push_back(std::move(new_pred));
    }
  }
  return new_action;
}

inline ParameterMapping get_mapping(const Action &action,
                                    const Condition &predicate) noexcept {
  std::vector<std::vector<const Argument *>> parameter_matches(
      action.parameters.size());
  for (const auto &argument : predicate.arguments) {
    if (!argument.is_constant()) {
      parameter_matches[get_index(&argument.get_parameter(), action.parameters)]
          .push_back(&argument);
    }
  }

  ParameterMapping mapping;
  for (size_t i = 0; i < action.parameters.size(); ++i) {
    if (!parameter_matches[i].empty()) {
      mapping.parameter_selection.parameters.push_back(&action.parameters[i]);
      mapping.arguments.push_back(std::move(parameter_matches[i]));
    }
  }
  mapping.parameter_selection.action = &action;
  mapping.condition = &predicate;
  return mapping;
}

inline ParameterAssignment
get_assignment(const ParameterMapping &mapping,
               const std::vector<const Constant *> &arguments) noexcept {
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
      [&problem](size_t product, const Type *type) {
        return product *
               problem.constants_by_type[get_index(type, problem)].size();
      });
}

inline size_t get_num_instantiated(const Action &action,
                                   const Problem &problem) noexcept {
  return std::accumulate(action.parameters.begin(), action.parameters.end(),
                         1ul, [&problem](size_t product, const Parameter &p) {
                           return p.is_constant()
                                      ? product
                                      : product *
                                            problem
                                                .constants_by_type[get_index(
                                                    &p.get_type(), problem)]
                                                .size();
                         });
}

inline size_t get_num_instantiated(const Condition &condition,
                                   const Problem &problem) noexcept {
  return std::accumulate(
      condition.arguments.begin(), condition.arguments.end(), 1ul,
      [&problem](size_t product, const Argument &a) {
        return a.is_constant()
                   ? product
                   : product * problem
                                   .constants_by_type[get_index(
                                       &a.get_parameter().get_type(), problem)]
                                   .size();
      });
}

template <typename Function>
void for_each_instantiation(const Predicate &predicate, Function &&f,
                            const Problem &problem) noexcept {
  std::vector<size_t> number_arguments;
  number_arguments.reserve(predicate.parameter_types.size());
  for (auto type : predicate.parameter_types) {
    number_arguments.push_back(
        problem.constants_by_type[get_index(type, problem)].size());
  }

  auto combination_iterator = CombinationIterator{std::move(number_arguments)};

  std::vector<const Constant *> arguments(predicate.parameter_types.size());
  while (!combination_iterator.end()) {
    const auto &combination = *combination_iterator;
    for (size_t i = 0; i < combination.size(); ++i) {
      auto type = predicate.parameter_types[i];
      auto constant =
          problem.constants_by_type[get_index(type, problem)][combination[i]];
      arguments[i] = constant;
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
  for (auto p : selection.parameters) {
    assert(!p->is_constant());
    argument_size_list.push_back(
        problem.constants_by_type[get_index(&p->get_type(), problem)].size());
  }

  auto combination_iterator = CombinationIterator{argument_size_list};

  while (!combination_iterator.end()) {
    const auto &combination = *combination_iterator;
    ParameterAssignment assignment;
    assignment.assignments.reserve(combination.size());
    for (size_t i = 0; i < combination.size(); ++i) {
      assert(!parameters[to_ground[i]].is_constant());
      auto type = &selection.parameters[i]->get_type();
      assignment.assignments.push_back(
          {selection.parameters[i],
           problem
               .constants_by_type[get_index(type, problem)][combination[i]]});
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

inline bool
is_instantiatable(const Condition &condition,
                  const std::vector<const Constant *> &instantiation,
                  const Action &action) noexcept {
  auto parameters = action.parameters;
  for (size_t i = 0; i < condition.arguments.size(); ++i) {
    if (condition.arguments[i].is_constant()) {
      if (&condition.arguments[i].get_constant() != instantiation[i]) {
        return false;
      }
    } else {
      auto &p = parameters[get_index(&condition.arguments[i].get_parameter(),
                                     action.parameters)];
      if (p.is_constant()) {
        if (&p.get_constant() != instantiation[i]) {
          return false;
        }
      } else {
        if (!is_subtype(*instantiation[i]->type, p.get_type())) {
          return false;
        }
        p.set(*instantiation[i]);
      }
    }
  }
  return true;
}

} // namespace normalized

#endif /* end of include guard: MODEL_UTILS_HPP */
