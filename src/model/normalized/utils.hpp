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

using ParameterSelection = std::vector<ParameterIndex>;

struct ParameterMapping {
  ParameterSelection parameters;
  std::vector<std::vector<ArgumentIndex>> arguments;
};

using ParameterAssignment =
    std::vector<std::pair<ParameterIndex, ConstantIndex>>;

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

inline bool is_grounded(const Condition &predicate) noexcept {
  return std::all_of(predicate.arguments.cbegin(), predicate.arguments.cend(),
                     [](const auto &arg) { return arg.is_constant(); });
}

inline Action ground(const ParameterAssignment &assignment,
                     const Action &action) noexcept {
  Action new_action;
  new_action.parameters = action.parameters;
  for (auto [p, c] : assignment) {
    new_action.parameters[p].set(c);
  }
  new_action.pre_instantiated = action.pre_instantiated;
  new_action.eff_instantiated = action.eff_instantiated;

  for (auto pred : action.preconditions) {
    if (bool is_grounded = update_arguments(pred, new_action); is_grounded) {
      new_action.pre_instantiated.emplace_back(instantiate(pred),
                                               pred.positive);
    } else {
      new_action.preconditions.push_back(std::move(pred));
    }
  }
  for (auto pred : action.effects) {
    if (bool is_grounded = update_arguments(pred, new_action); is_grounded) {
      new_action.eff_instantiated.emplace_back(instantiate(pred),
                                               pred.positive);
    } else {
      new_action.effects.push_back(std::move(pred));
    }
  }
  return new_action;
}

inline ParameterMapping get_mapping(const Action &action,
                                    const Condition &predicate) noexcept {
  std::vector<std::vector<ArgumentIndex>> parameter_matches(
      action.parameters.size());
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    if (!predicate.arguments[i].is_constant()) {
      parameter_matches[predicate.arguments[i].get_parameter()].emplace_back(i);
    }
  }

  ParameterMapping mapping;
  for (size_t i = 0; i < action.parameters.size(); ++i) {
    if (!parameter_matches[i].empty()) {
      mapping.parameters.emplace_back(i);
      mapping.arguments.push_back(std::move(parameter_matches[i]));
    }
  }
  return mapping;
}

inline ParameterSelection
get_referenced_parameters(const Action &action,
                          const Condition &predicate) noexcept {
  std::vector<bool> parameter_matches(action.parameters.size(), false);
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    if (!predicate.arguments[i].is_constant()) {
      parameter_matches[predicate.arguments[i].get_parameter()] = true;
    }
  }

  ParameterSelection selection;
  for (size_t i = 0; i < action.parameters.size(); ++i) {
    if (parameter_matches[i]) {
      selection.emplace_back(i);
    }
  }
  return selection;
}

inline ParameterAssignment
get_assignment(const ParameterMapping &mapping,
               const std::vector<ConstantIndex> &arguments) noexcept {
  assert(mapping.parameters.size() == arguments.size());
  ParameterAssignment assignment;
  assignment.reserve(mapping.parameters.size());
  for (size_t i = 0; i < mapping.parameters.size(); ++i) {
    assignment.push_back({mapping.parameters[i], arguments[i]});
  }
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
void for_each_instantiation(const ParameterMapping &mapping,
                            const Condition &condition, const Action &action,
                            Function &&f, const Problem &problem) {
  std::vector<size_t> argument_size_list;
  argument_size_list.reserve(mapping.parameters.size());
  for (auto p : mapping.parameters) {
    assert(!action.get(p).is_constant());
    argument_size_list.push_back(
        problem.constants_by_type[action.get(p).get_type()].size());
  }

  auto combination_iterator = CombinationIterator{argument_size_list};

  while (!combination_iterator.end()) {
    Condition new_condition = condition;
    const auto &combination = *combination_iterator;
    for (size_t i = 0; i < combination.size(); ++i) {
      assert(!action.get(mapping.parameters[i]).is_constant());
      auto type = action.get(mapping.parameters[i]).get_type();
      for (auto a : mapping.arguments[i]) {
        assert(new_condition.arguments[a].get_parameter() ==
               mapping.parameters[i]);
        new_condition.arguments[a].set(
            problem.constants_by_type[type][combination[i]]);
      }
    }
    f(instantiate(new_condition));
    ++combination_iterator;
  }
}

template <typename Function>
void for_each_instantiation(const ParameterSelection &selection,
                            const Action &action, Function &&f,
                            const Problem &problem) {
  std::vector<size_t> argument_size_list;
  argument_size_list.reserve(selection.size());
  for (auto p : selection) {
    assert(!action.get(p).is_constant());
    argument_size_list.push_back(
        problem.constants_by_type[action.get(p).get_type()].size());
  }

  auto combination_iterator = CombinationIterator{argument_size_list};

  while (!combination_iterator.end()) {
    const auto &combination = *combination_iterator;
    ParameterAssignment assignment;
    assignment.reserve(combination.size());
    for (size_t i = 0; i < combination.size(); ++i) {
      assert(!action.get(selection[i]).is_constant());
      auto type = action.get(selection[i]).get_type();
      assignment.emplace_back(selection[i],
                              problem.constants_by_type[type][combination[i]]);
    }
    f(std::move(assignment));
    ++combination_iterator;
  }
}

inline bool is_instantiatable(const Condition &condition,
                              const std::vector<ConstantIndex> &arguments,
                              const Action &action,
                              const Problem &problem) noexcept {
  auto parameters = action.parameters;
  for (size_t i = 0; i < condition.arguments.size(); ++i) {
    if (condition.arguments[i].is_constant()) {
      if (condition.arguments[i].get_constant() != arguments[i]) {
        return false;
      }
    } else {
      auto &p = parameters[condition.arguments[i].get_parameter()];
      if (p.is_constant()) {
        if (p.get_constant() != arguments[i]) {
          return false;
        }
      } else {
        if (!is_subtype(problem.get(arguments[i]).type, p.get_type(),
                        problem)) {
          return false;
        }
        p.set(arguments[i]);
      }
    }
  }
  return true;
}

} // namespace normalized

#endif /* end of include guard: NORMALIZED_UTILS_HPP */
