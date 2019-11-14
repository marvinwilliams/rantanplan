#ifndef MODEL_UTILS_HPP
#define MODEL_UTILS_HPP

#include "model/model.hpp"
#include "util/combinatorics.hpp"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <variant>
#include <vector>

namespace model {

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

inline PredicateInstantiation
instantiate(const ConditionPredicate &predicate) noexcept {
  std::vector<ConstantHandle> args;
  args.reserve(predicate.arguments.size());
  for (const auto &arg : predicate.arguments) {
    assert(arg.constant);
    args.push_back(ConstantHandle{arg.index});
  }
  return PredicateInstantiation{predicate.definition, std::move(args)};
}

inline PredicateInstantiation
instantiate(const ConditionPredicate &predicate,
            const std::vector<Parameter> &parameters) noexcept {
  std::vector<ConstantHandle> args;
  args.reserve(predicate.arguments.size());
  for (const auto &arg : predicate.arguments) {
    if (arg.constant) {
      args.push_back(ConstantHandle{arg.index});
    } else {
      assert(parameters[arg.index].constant);
      args.push_back(ConstantHandle{parameters[arg.index].index});
    }
  }
  return PredicateInstantiation{predicate.definition, std::move(args)};
}

inline std::pair<ConditionPredicate, bool>
ground(const ConditionPredicate &predicate,
       const std::vector<Parameter> &parameters) {
  ConditionPredicate new_predicate;
  new_predicate.definition = predicate.definition;
  new_predicate.negated = predicate.negated;

  bool grounded = true;
  for (auto &arg : predicate.arguments) {
    if (arg.constant) {
      new_predicate.arguments.push_back(arg);
    } else {
      if (parameters[arg.index].constant) {
        new_predicate.arguments.push_back(parameters[arg.index]);
      } else {
        new_predicate.arguments.push_back(arg);
        grounded = false;
      }
    }
  }
  return {std::move(new_predicate), grounded};
}

inline Action ground(const Action &action,
                     const ParameterAssignment &assignment) noexcept {
  Action new_action;
  new_action.parameters = action.parameters;
  for (const auto &[p, c] : assignment) {
    new_action.parameters[p].constant = true;
    new_action.parameters[p].index = c;
  }
  new_action.pre_instantiated = action.pre_instantiated;
  new_action.eff_instantiated = action.eff_instantiated;
  for (const auto &pred : action.preconditions) {
    if (auto [new_pred, grounded] = ground(pred, action.parameters); grounded) {
      new_action.pre_instantiated.emplace_back(instantiate(new_pred),
                                               pred.negated);
    } else {
      new_action.preconditions.push_back(std::move(new_pred));
    }
  }
  for (const auto &pred : action.effects) {
    if (auto [new_pred, grounded] = ground(pred, action.parameters); grounded) {
      new_action.eff_instantiated.emplace_back(instantiate(new_pred),
                                               pred.negated);
    } else {
      new_action.effects.push_back(std::move(new_pred));
    }
  }
  return new_action;
}

inline bool is_grounded(const ConditionPredicate &predicate) noexcept {
  return std::all_of(predicate.arguments.cbegin(), predicate.arguments.cend(),
                     [](const auto &arg) { return arg.constant; });
}

inline bool is_grounded(const ConditionPredicate &predicate,
                        const std::vector<Parameter> &parameters) noexcept {
  return std::all_of(predicate.arguments.cbegin(), predicate.arguments.cend(),
                     [&parameters](const auto &arg) {
                       return arg.constant || parameters[arg.index].constant;
                     });
}

inline ParameterMapping
get_mapping(const std::vector<Parameter> &parameters,
            const ConditionPredicate &predicate) noexcept {
  std::vector<std::vector<ParameterHandle>> parameter_matches{
      parameters.size()};
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    if (!predicate.arguments[i].constant) {
      parameter_matches[predicate.arguments[i].index].push_back(
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
  assert(arguments.size() == mapping.size());
  ParameterAssignment assignment;
  for (size_t i = 0; i < mapping.size(); ++i) {
    assert(mapping[i].first < action.parameters.size());
    assignment.emplace_back(mapping[i].first, arguments[i]);
  }
  return assignment;
}

inline PredicateInstantiationHandle
get_handle(const PredicateInstantiation &predicate, size_t num_constants) {
  size_t factor = 1;
  size_t result = 0;
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    assert(result <= result + arguments[i] * factor); // Overflow
    result += predicate.arguments[i] * factor;
    factor *= num_constants;
  }
  return PredicateInstantiationHandle{result};
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
        return param.constant ? product
                              : product * constants_by_type[param.index].size();
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
void for_each_assignment(const ConditionPredicate &predicate,
                         const std::vector<Parameter> &parameters, Function f,
                         const std::vector<std::vector<ConstantHandle>>
                             &constants_by_type) noexcept {
  std::vector<ConstantHandle> arguments(predicate.arguments.size());
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    if (predicate.arguments[i].constant) {
      arguments[i] = ConstantHandle{predicate.arguments[i].index};
    }
  }

  ParameterMapping mapping = get_mapping(parameters, predicate);

  std::vector<size_t> argument_sizes;
  argument_sizes.reserve(mapping.size());
  for (const auto &p : mapping) {
    assert(!parameters[p.first].constant);
    argument_sizes.push_back(
        constants_by_type[parameters[p.first].index].size());
  }

  auto combination_iterator = CombinationIterator{argument_sizes};

  while (!combination_iterator.end()) {
    const auto &combination = *combination_iterator;
    std::vector<ConstantHandle> constants(combination.size());
    for (size_t i = 0; i < combination.size(); ++i) {
      assert(!parameters[mapping[i].first].constant);
      constants.push_back(constants_by_type[parameters[mapping[i].first].index]
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
    if (first[i].constant && second[i].constant &&
        first[i].index != second[i].index) {
      return false;
    }
  }
  return true;
}

// Copy parameters to try to find the grounding
inline bool is_instantiatable(const ConditionPredicate &predicate,
                              const std::vector<ConstantHandle> &instantiation,
                              std::vector<Parameter> parameters,
                              const std::vector<Constant> &constants,
                              const std::vector<Type> &types) noexcept {
  assert(predicate.arguments.size() == arguments.size());
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    if (predicate.arguments[i].constant) {
      if (predicate.arguments[i].index != instantiation[i]) {
        return false;
      }
    } else {
      ParameterHandle handle = ParameterHandle{predicate.arguments[i].index};
      if (parameters[handle].constant) {
        if (ConstantHandle{parameters[handle].index} != instantiation[i]) {
          return false;
        }
      } else {
        if (!is_subtype(constants[instantiation[i]].type,
                        TypeHandle{parameters[handle].index}, types)) {
          return false;
        }
        parameters[handle].constant = true;
        parameters[handle].index = instantiation[i];
      }
    }
  }
  return true;
}

} // namespace model

#endif /* end of include guard: MODEL_UTILS_HPP */
