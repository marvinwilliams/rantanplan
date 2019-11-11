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

inline bool is_subtype(const ProblemBase &problem, TypeHandle type,
                       TypeHandle supertype) {
  if (type == supertype) {
    return true;
  }
  while (problem.types[type].parent != type) {
    type = problem.types[type].parent;
    if (type == supertype) {
      return true;
    }
  }
  return false;
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
    if (auto parameter_handle =
            std::get_if<ParameterHandle>(&predicate.arguments[i])) {
      parameter_matches[*parameter_handle].push_back(ParameterHandle{i});
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
    auto parameter_handle =
        std::get_if<ParameterHandle>(&predicate.arguments[i]);
    if (parameter_handle && !assignment[*parameter_handle]) {
      parameter_matches[*parameter_handle].push_back(ParameterHandle{i});
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
get_assignment(const Action &action, const ParameterMapping &mapping,
               const std::vector<size_t> &arguments) noexcept {
  assert(arguments.size() == mapping.size());
  ParameterAssignment assignment;
  assignment.resize(action.parameters.size());
  for (size_t i = 0; i < mapping.size(); ++i) {
    assert(mapping[i].first < action.parameters.size());
    assignment[mapping[i].first] = arguments[i];
  }
  return assignment;
}

inline GroundPredicateHandle
get_ground_predicate_handle(const Problem &problem, PredicateHandle handle,
                            const std::vector<ConstantHandle> &arguments) {
  const PredicateDefinition &predicate = problem.predicates[handle];
  size_t factor = 1;
  size_t result = 0;
  for (size_t i = 0; i < predicate.parameters.size(); ++i) {
    assert(result <= result + arguments[i] * factor);
    result += arguments[i] * factor;
    factor *= problem.constants.size();
  }
  return GroundPredicateHandle{result};
}

inline size_t get_num_grounded(const Problem &problem,
                               PredicateHandle predicate_ptr) {
  const PredicateDefinition &predicate = problem.predicates[predicate_ptr];
  return std::accumulate(
      predicate.parameters.begin(), predicate.parameters.end(), 1ul,
      [&problem](size_t product, const Parameter &parameter) {
        return product * problem.constants_by_type[parameter.type].size();
      });
}

inline size_t get_num_grounded(const Problem &problem, const Action &action,
                               const ParameterAssignment &assignment,
                               const PredicateEvaluation &predicate) {
  ParameterMapping mapping = get_mapping(action, assignment, predicate);

  return std::accumulate(
      mapping.begin(), mapping.end(), 1ul,
      [&problem, &action](size_t product, const auto &m) {
        return product *
               problem.constants_by_type[action.parameters[m.first].type]
                   .size();
      });
}

template <typename Function>
void for_grounded_predicate(const Problem &problem,
                            PredicateHandle predicate_ptr, Function f) {
  const PredicateDefinition &predicate = problem.predicates[predicate_ptr];
  std::vector<size_t> number_arguments;
  number_arguments.reserve(predicate.parameters.size());
  for (const auto &parameter : predicate.parameters) {
    number_arguments.push_back(
        problem.constants_by_type[parameter.type].size());
  }

  auto combination_iterator = CombinationIterator{std::move(number_arguments)};

  while (!combination_iterator.end()) {
    const auto &combination = *combination_iterator;
    std::vector<ConstantHandle> arguments;
    arguments.reserve(combination.size());
    for (size_t i = 0; i < combination.size(); ++i) {
      auto type = predicate.parameters[i].type;
      auto constant = problem.constants_by_type[type][combination[i]];
      arguments.push_back(constant);
    }
    GroundPredicate ground_predicate{predicate_ptr, std::move(arguments)};
    f(std::move(ground_predicate));
    ++combination_iterator;
  }
}

template <typename Function>
void for_grounded_predicate(const Problem &problem, const Action &action,
                            const ParameterAssignment &assignment,
                            const PredicateEvaluation &predicate, Function f) {
  std::vector<ConstantHandle> arguments(predicate.arguments.size());
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    const auto &argument = predicate.arguments[i];
    if (auto constant_handle = std::get_if<ConstantHandle>(&argument)) {
      arguments[i] = *constant_handle;
    } else {
      auto parameter_handle = std::get<ParameterHandle>(argument);
      auto type = action.parameters[parameter_handle].type;
      if (assignment[parameter_handle]) {
        arguments[i] =
            problem.constants_by_type[type][*assignment[parameter_handle]];
      }
    }
  }

  ParameterMapping mapping = get_mapping(action, assignment, predicate);

  std::vector<size_t> argument_sizes;
  argument_sizes.reserve(mapping.size());
  for (const auto &[parameter_handle, predicate_parameters] : mapping) {
    argument_sizes.push_back(
        problem.constants_by_type[action.parameters[parameter_handle].type]
            .size());
  }

  auto combination_iterator = CombinationIterator{argument_sizes};

  GroundPredicate ground_predicate{predicate.definition, std::move(arguments)};

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
