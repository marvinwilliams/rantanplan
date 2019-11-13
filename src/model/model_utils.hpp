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

inline bool is_subtype(TypeHandle type, TypeHandle supertype,
                       const std::vector<Type> &types) noexcept {
  if (type == supertype) {
    return true;
  }
  while (types[type].parent != type) {
    type = types[type].parent;
    if (type == supertype) {
      return true;
    }
  }
  return false;
}

inline GroundPredicate
instantiate(const PredicateEvaluation &predicate) noexcept {
  std::vector<ConstantHandle> arguments;
  arguments.reserve(predicate.arguments.size());
  for (const auto &argument : predicate.arguments) {
    assert(argument.type == Argument::Type::Constant);
    arguments.emplace_back(argument.handle);
  }
  return GroundPredicate{predicate.definition, std::move(arguments)};
}

inline GroundPredicate
instantiate(const PredicateEvaluation &predicate,
            const ParameterAssignment &assignment) noexcept {
  std::vector<ConstantHandle> arguments;
  arguments.reserve(predicate.arguments.size());
  for (const auto &argument : predicate.arguments) {
    if (argument.handle_type == Argument::Type::Constant) {
      arguments.emplace_back(argument.handle);
    } else {
      assert(assignment.find(ParameterHandle{argument.handle}) !=
             assignment.end());
      arguments.push_back(assignment.at(ParameterHandle{argument.handle}));
    }
  }
  return GroundPredicate{predicate.definition, std::move(arguments)};
}

inline bool is_grounded(const PredicateEvaluation &predicate) noexcept {
  return std::all_of(
      predicate.arguments.cbegin(), predicate.arguments.cend(),
      [](const auto &a) { return std::holds_alternative<ConstantHandle>(a); });
}

inline bool is_grounded(const ParameterAssignment &assignment,
                        const PredicateEvaluation &predicate) noexcept {
  return std::all_of(predicate.arguments.cbegin(), predicate.arguments.cend(),
                     [&assignment](const auto &a) {
                       return std::holds_alternative<ConstantHandle>(a) ||
                              assignment[std::get<ParameterHandle>(a)];
                     });
}

inline ParameterMapping
get_mapping(const Action &action,
            const PredicateEvaluation &predicate) noexcept {
  std::vector<std::vector<ParameterHandle>> parameter_matches{
      action.parameters.size()};
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    if (predicate.arguments[i].handle_type == Argument::Type::Parameter) {
      parameter_matches[predicate.arguments[i].handle].push_back(
          ParameterHandle{i});
    }
  }

  ParameterMapping mapping;
  for (size_t i = 0; i < action.parameters.size(); ++i) {
    if (!parameter_matches[i].empty()) {
      mapping.emplace_back(ParameterHandle{i}, std::move(parameter_matches[i]));
    }
  }

  return mapping;
}

inline ParameterMapping
get_mapping(const Action &action, const ParameterAssignment &assignment,
            const PredicateEvaluation &predicate) noexcept {
  std::vector<std::vector<ParameterHandle>> parameter_matches{
      action.parameters.size()};
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    if (predicate.arguments[i].handle_type == Argument::Type::Parameter) {
      ParameterHandle handle{predicate.arguments[i].handle};
      if (assignment.find(handle) != assignment.end()) {
        parameter_matches[handle].push_back(ParameterHandle{i});
      }
    }
  }

  ParameterMapping mapping;
  for (size_t i = 0; i < action.parameters.size(); ++i) {
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
    assignment[mapping[i].first] = arguments[i];
  }
  return assignment;
}

inline GroundPredicateHandle get_handle(const GroundPredicate &predicate,
                                        size_t num_constants) {
  size_t factor = 1;
  size_t result = 0;
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    assert(result <= result + arguments[i] * factor); // Overflow
    result += predicate.arguments[i] * factor;
    factor *= num_constants;
  }
  return GroundPredicateHandle{result};
}

inline size_t get_num_grounded(const PredicateDefinition &predicate,
                               const std::vector<std::vector<ConstantHandle>>
                                   &constants_by_type) noexcept {
  return std::accumulate(
      predicate.parameters.begin(), predicate.parameters.end(), 1ul,
      [&constants_by_type](size_t product, const Parameter &parameter) {
        return product * constants_by_type[parameter.type].size();
      });
}

inline size_t get_num_grounded(
    const ParameterMapping &mapping, const Action &action,
    const std::vector<std::vector<ConstantHandle>> &constants_by_type) {

  return std::accumulate(
      mapping.begin(), mapping.end(), 1ul,
      [&action, &constants_by_type](size_t product, const auto &m) {
        return product *
               constants_by_type[action.parameters[m.first].type].size();
      });
}

template <typename Function>
void for_each_instantiation(const PredicateDefinition &predicate, Function f,
                            const std::vector<std::vector<ConstantHandle>>
                                &constants_by_type) noexcept {
  std::vector<size_t> number_arguments;
  number_arguments.reserve(predicate.parameters.size());
  for (const auto &parameter : predicate.parameters) {
    number_arguments.push_back(constants_by_type[parameter.type].size());
  }

  auto combination_iterator = CombinationIterator{std::move(number_arguments)};

  std::vector<ConstantHandle> arguments(predicate.parameters.size());
  while (!combination_iterator.end()) {
    const auto &combination = *combination_iterator;
    for (size_t i = 0; i < combination.size(); ++i) {
      auto type = predicate.parameters[i].type;
      auto constant = constants_by_type[type][combination[i]];
      arguments[i] = constant;
    }
    f(arguments);
    ++combination_iterator;
  }
}

template <typename Function>
void for_each_assignment(const PredicateEvaluation &predicate,
                            const Action &action,
                            const ParameterAssignment &assignment, Function f,
                            const std::vector<std::vector<ConstantHandle>>
                                &constants_by_type) noexcept {
  std::vector<ConstantHandle> arguments(predicate.arguments.size());
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    if (predicate.arguments[i].handle_type == Argument::Type::Constant) {
      arguments[i] = ConstantHandle{predicate.arguments[i].handle};
    } else {
      ParameterHandle handle{predicate.arguments[i].handle};
      if (auto it = assignment.find(handle); it != assignment.end()) {
        arguments[i] = it->second;
      }
    }
  }

  ParameterMapping mapping = get_mapping(action, assignment, predicate);

  std::vector<size_t> argument_sizes;
  argument_sizes.reserve(mapping.size());
  for (const auto &[parameter_handle, predicate_parameters] : mapping) {
    argument_sizes.push_back(
        constants_by_type[action.parameters[parameter_handle].type]
            .size());
  }

  auto combination_iterator = CombinationIterator{argument_sizes};

  while (!combination_iterator.end()) {
    const auto &combination = *combination_iterator;
    f(get_assignment(action, mapping, combination));
    ++combination_iterator;
  }
}

inline bool is_unifiable(const ParameterAssignment &first,
                         const ParameterAssignment &second) noexcept {
  assert(first.size() == second.size());
  for (size_t i = 0; i < first.size(); ++i) {
    if (first[i] && second[i] && *first[i] != *second[i]) {
      return false;
    }
  }
  return true;
}

inline bool is_groundable(const Problem &problem, const Action &action,
                          const ParameterAssignment &assignment,
                          const PredicateEvaluation &predicate,
                          const std::vector<ConstantHandle> &arguments) {
  assert(predicate.arguments.size() == arguments.size());
  std::vector<std::optional<ConstantHandle>> grounded_parameters(
      action.parameters.size());
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    auto expected_argument = arguments[i];
    if (auto p = std::get_if<ConstantHandle>(&predicate.arguments[i])) {
      if (*p != expected_argument) {
        return false;
      }
    } else {
      auto parameter_handle = std::get<ParameterHandle>(predicate.arguments[i]);
      if (assignment[parameter_handle]) {
        if (problem.constants_by_type[action.parameters[parameter_handle].type]
                                     [*assignment[parameter_handle]] !=
            expected_argument) {
          return false;
        }
      } else {
        if (grounded_parameters[parameter_handle]) {
          if (*grounded_parameters[parameter_handle] != expected_argument) {
            return false;
          }
        } else {
          if (!is_subtype(problem, problem.constants[expected_argument].type,
                          action.parameters[parameter_handle].type)) {
            return false;
          }
          grounded_parameters[parameter_handle] = expected_argument;
        }
      }
    }
  }
  return true;
}

} // namespace model

#endif /* end of include guard: MODEL_UTILS_HPP */
