#ifndef MODEL_UTILS_HPP
#define MODEL_UTILS_HPP

#include "model/normalized_problem.hpp"
#include "util/combinatorics.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>
#include <variant>
#include <vector>

namespace normalized {

using ParameterMapping =
    std::vector<std::pair<ParameterHandle, std::vector<ParameterHandle>>>;
using ParameterAssignment =
    std::vector<std::pair<ParameterHandle, ConstantHandle>>;

inline bool is_subtype(TypeHandle subtype, TypeHandle type,
                       const std::vector<Type> &types) noexcept {
  if (subtype == type) {
    return true;
  }
  while (types[subtype].parent != subtype) {
    subtype = types[subtype].parent;
    if (subtype == type) {
      return true;
    }
  }
  return false;
}

inline PredicateInstantiation instantiate(const Condition &predicate) noexcept {
  std::vector<ConstantHandle> args;
  args.reserve(predicate.arguments.size());
  for (const auto &arg : predicate.arguments) {
    assert(arg.is_constant());
    args.push_back(arg.get_constant());
  }
  return PredicateInstantiation{predicate.definition, std::move(args)};
}

inline PredicateInstantiation
instantiate(const Condition &predicate,
            const std::vector<Parameter> &parameters) noexcept {
  std::vector<ConstantHandle> args;
  args.reserve(predicate.arguments.size());
  for (const auto &arg : predicate.arguments) {
    if (arg.is_constant()) {
      args.push_back(arg.get_constant());
    } else {
      assert(parameters[arg.get_parameter()].is_constant());
      args.push_back(parameters[arg.get_parameter()].get_constant());
    }
  }
  return PredicateInstantiation{predicate.definition, std::move(args)};
}

inline std::pair<Condition, bool>
ground(const Condition &predicate, const std::vector<Parameter> &parameters) {
  Condition new_predicate;
  new_predicate.definition = predicate.definition;
  new_predicate.positive = predicate.positive;

  bool grounded = true;
  for (const auto &arg : predicate.arguments) {
    if (arg.is_constant()) {
      new_predicate.arguments.emplace_back(arg.get_constant());
    } else if (parameters[arg.get_parameter()].is_constant()) {
      new_predicate.arguments.emplace_back(
          parameters[arg.get_parameter()].get_constant());
    } else {
      new_predicate.arguments.emplace_back(arg.get_parameter());
      grounded = false;
    }
  }
  return {std::move(new_predicate), grounded};
}

inline Action ground(const Action &action,
                     const ParameterAssignment &assignment) noexcept {
  Action new_action;
  new_action.parameters = action.parameters;
  for (const auto &[p, c] : assignment) {
    new_action.parameters[p].set(c);
  }
  new_action.pre_instantiated = action.pre_instantiated;
  new_action.eff_instantiated = action.eff_instantiated;
  for (const auto &pred : action.preconditions) {
    if (auto [new_pred, grounded] = ground(pred, new_action.parameters);
        grounded) {
      new_action.pre_instantiated.emplace_back(instantiate(new_pred),
                                               pred.positive);
    } else {
      new_action.preconditions.push_back(std::move(new_pred));
    }
  }
  for (const auto &pred : action.effects) {
    if (auto [new_pred, grounded] = ground(pred, new_action.parameters);
        grounded) {
      new_action.eff_instantiated.emplace_back(instantiate(new_pred),
                                               pred.positive);
    } else {
      new_action.effects.push_back(std::move(new_pred));
    }
  }
  return new_action;
}

inline bool is_grounded(const Condition &predicate) noexcept {
  return std::all_of(predicate.arguments.cbegin(), predicate.arguments.cend(),
                     [](const auto &arg) { return arg.is_constant(); });
}

inline bool is_grounded(const Condition &predicate,
                        const std::vector<Parameter> &parameters) noexcept {
  return std::all_of(predicate.arguments.cbegin(), predicate.arguments.cend(),
                     [&parameters](const auto &arg) {
                       return arg.is_constant() ||
                              parameters[arg.get_parameter()].is_constant();
                     });
}

inline ParameterMapping get_mapping(const std::vector<Parameter> &parameters,
                                    const Condition &predicate) noexcept {
  std::vector<std::vector<ParameterHandle>> parameter_matches(
      parameters.size());
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    if (!predicate.arguments[i].is_constant()) {
      assert(!parameters[predicate.arguments[i].get_parameter()].is_constant());
      parameter_matches[predicate.arguments[i].get_parameter()].push_back(
          ParameterHandle{i});
    }
  }

  ParameterMapping mapping;
  for (size_t i = 0; i < parameters.size(); ++i) {
    if (!parameter_matches[i].empty()) {
      mapping.emplace_back(ParameterHandle{i}, std::move(parameter_matches[i]));
    }
  }

  return mapping;
}

inline ParameterAssignment
get_assignment(const ParameterMapping &mapping,
               const std::vector<ConstantHandle> &arguments) noexcept {
  assert(mapping.size() == arguments.size());
  ParameterAssignment assignment;
  for (size_t i = 0; i < mapping.size(); ++i) {
    assignment.emplace_back(mapping[i].first, arguments[i]);
  }
  return assignment;
}

inline InstantiationHandle get_handle(const PredicateInstantiation &predicate,
                                      size_t num_constants) {
  size_t factor = 1;
  size_t result = 0;
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    assert(result <= result + predicate.arguments[i] * factor); // Overflow
    result += predicate.arguments[i] * factor;
    factor *= num_constants;
  }
  return InstantiationHandle{result};
}

inline size_t get_num_grounded(
    const Predicate &predicate,
    const std::vector<std::vector<ConstantHandle>> &constants_by_type) {

  return std::accumulate(
      predicate.parameter_types.begin(), predicate.parameter_types.end(), 1ul,
      [&constants_by_type](size_t product, TypeHandle handle) {
        return product * constants_by_type[handle].size();
      });
}

inline size_t get_num_grounded(const std::vector<Parameter> &parameters,
                               const std::vector<std::vector<ConstantHandle>>
                                   &constants_by_type) noexcept {
  return std::accumulate(
      parameters.begin(), parameters.end(), 1ul,
      [&constants_by_type](size_t product, const Parameter &param) {
        return param.is_constant()
                   ? product
                   : product * constants_by_type[param.get_type()].size();
      });
}

inline size_t get_num_grounded(const std::vector<Argument> &arguments,
                               const std::vector<Parameter> &parameters,
                               const std::vector<std::vector<ConstantHandle>>
                                   &constants_by_type) noexcept {
  return std::accumulate(
      arguments.begin(), arguments.end(), 1ul,
      [&parameters, &constants_by_type](size_t product, const Argument &arg) {
        return arg.is_constant()
                   ? product
                   : product * constants_by_type[parameters[arg.get_parameter()]
                                                     .get_type()]
                                   .size();
      });
}
template <typename Function>
void for_each_instantiation(const Predicate &predicate, Function f,
                            const std::vector<std::vector<ConstantHandle>>
                                &constants_by_type) noexcept {
  std::vector<size_t> number_arguments;
  number_arguments.reserve(predicate.parameter_types.size());
  for (auto type : predicate.parameter_types) {
    number_arguments.push_back(constants_by_type[type].size());
  }

  auto combination_iterator = CombinationIterator{std::move(number_arguments)};

  std::vector<ConstantHandle> arguments(predicate.parameter_types.size());
  while (!combination_iterator.end()) {
    const auto &combination = *combination_iterator;
    for (size_t i = 0; i < combination.size(); ++i) {
      auto type = predicate.parameter_types[i];
      auto constant = constants_by_type[type][combination[i]];
      arguments[i] = constant;
    }
    f(arguments);
    ++combination_iterator;
  }
}

template <typename Function>
void for_each_assignment(const Condition &predicate,
                         const std::vector<Parameter> &parameters, Function f,
                         const std::vector<std::vector<ConstantHandle>>
                             &constants_by_type) noexcept {
  std::vector<ConstantHandle> arguments(predicate.arguments.size());
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    if (predicate.arguments[i].is_constant()) {
      arguments[i] = predicate.arguments[i].get_constant();
    }
  }

  ParameterMapping mapping = get_mapping(parameters, predicate);

  std::vector<size_t> argument_sizes;
  argument_sizes.reserve(mapping.size());
  for (const auto &p : mapping) {
    assert(!parameters[p.first].is_constant());
    argument_sizes.push_back(
        constants_by_type[parameters[p.first].get_type()].size());
  }

  auto combination_iterator = CombinationIterator{argument_sizes};

  while (!combination_iterator.end()) {
    const auto &combination = *combination_iterator;
    std::vector<ConstantHandle> constants;
    constants.reserve(combination.size());
    for (size_t i = 0; i < combination.size(); ++i) {
      assert(!parameters[mapping[i].first].is_constant());
      constants.push_back(
          constants_by_type[parameters[mapping[i].first].get_type()]
                           [combination[i]]);
    }
    f(get_assignment(mapping, constants));
    ++combination_iterator;
  }
}

inline bool is_unifiable(const std::vector<Parameter> &first,
                         const std::vector<Parameter> &second) noexcept {
  // Assumes same action, thus free parameters are not checked
  assert(first.size() == second.size());
  for (size_t i = 0; i < first.size(); ++i) {
    if (first[i].is_constant() && second[i].is_constant() &&
        first[i].get_constant() != second[i].get_constant()) {
      return false;
    }
  }
  return true;
}

// Copy parameters to try to find the grounding
inline bool is_instantiatable(const Condition &predicate,
                              const std::vector<ConstantHandle> &instantiation,
                              std::vector<Parameter> parameters,
                              const std::vector<Constant> &constants,
                              const std::vector<Type> &types) noexcept {
  assert(predicate.arguments.size() == instantiation.size());
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    if (predicate.arguments[i].is_constant()) {
      if (predicate.arguments[i].get_constant() != instantiation[i]) {
        return false;
      }
    } else {
      auto handle = predicate.arguments[i].get_parameter();
      if (parameters[handle].is_constant()) {
        if (parameters[handle].get_constant() != instantiation[i]) {
          return false;
        }
      } else {
        if (!is_subtype(constants[instantiation[i]].type,
                        parameters[handle].get_type(), types)) {
          return false;
        }
        parameters[handle].set(instantiation[i]);
      }
    }
  }
  return true;
}

} // namespace normalized

#endif /* end of include guard: MODEL_UTILS_HPP */
