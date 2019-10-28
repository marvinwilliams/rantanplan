#ifndef MODEL_UTILS_HPP
#define MODEL_UTILS_HPP

#include "model/model.hpp"
#include "util/combinatorics.hpp"

#include <algorithm>
#include <cassert>
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
                              assignment.count(std::get<ParameterHandle>(a)) >
                                  0;
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
    if (parameter_handle && assignment.count(*parameter_handle) == 0) {
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
get_assignment(const ParameterMapping &mapping,
               const std::vector<size_t> &arguments) noexcept {
  ParameterAssignment assignment;
  assert(arguments.size() == mapping.size());
  assignment.reserve(mapping.size());
  for (size_t i = 0; i < mapping.size(); ++i) {
    assignment.insert({mapping[i].first, arguments[i]});
  }
  return assignment;
}

template <typename Function>
void for_grounded_predicate(const Problem &problem,
                            model::PredicateHandle predicate_ptr, Function f) {
  const model::PredicateDefinition &predicate =
      problem.predicates[predicate_ptr];
  std::vector<size_t> number_arguments;
  number_arguments.reserve(predicate.parameters.size());
  for (const auto &parameter : predicate.parameters) {
    number_arguments.push_back(
        problem.constants_by_type[parameter.type].size());
  }

  auto combination_iterator = CombinationIterator{std::move(number_arguments)};

  while (!combination_iterator.end()) {
    const auto &combination = *combination_iterator;
    std::vector<model::ConstantHandle> arguments;
    arguments.reserve(combination.size());
    for (size_t i = 0; i < combination.size(); ++i) {
      auto type = predicate.parameters[i].type;
      auto constant = problem.constants_by_type[type][combination[i]];
      arguments.push_back(constant);
    }
    model::GroundPredicate ground_predicate{predicate_ptr,
                                            std::move(arguments)};
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
    if (auto constant_handle = std::get_if<model::ConstantHandle>(&argument)) {
      arguments[i] = *constant_handle;
    } else {
      auto parameter_handle = std::get<model::ParameterHandle>(argument);
      auto type = action.parameters[parameter_handle].type;
      if (assignment.count(parameter_handle) > 0) {
        arguments[i] =
            problem.constants_by_type[type][assignment.at(parameter_handle)];
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

  model::GroundPredicate ground_predicate{predicate.definition,
                                          std::move(arguments)};

  while (!combination_iterator.end()) {
    const auto &combination = *combination_iterator;
    for (size_t i = 0; i < mapping.size(); ++i) {
      const auto &[parameter_handle, predicate_parameters] = mapping[i];
      auto type = action.parameters[parameter_handle].type;
      for (auto predicate_parameter_handle : predicate_parameters) {
        ground_predicate.arguments[predicate_parameter_handle] =
            problem.constants_by_type[type][combination[i]];
      }
    }
    f(ground_predicate, get_assignment(mapping, combination));
    ++combination_iterator;
  }
}

inline bool is_unifiable(const ParameterAssignment &first,
                         const ParameterAssignment &second) noexcept {
  return std::all_of(first.begin(), first.end(), [&second](auto a) {
    return (second.count(a.first) == 0 || second.at(a.first) == a.second);
  });
}

inline bool is_groundable(const Problem &problem, const Action &action,
                          const ParameterAssignment &assignment,
                          const PredicateEvaluation &predicate,
                          const std::vector<ConstantHandle> &arguments) {
  assert(predicate.arguments.size() == arguments.size());
  std::unordered_map<ParameterHandle, ConstantHandle, hash::Handle<Parameter>>
      grounded_parameters;
  // Possible speedup (lookup in both maps instead of fusing)
  for (auto [parameter_handle, constant_index] : assignment) {
    TypeHandle type_handle = action.parameters[parameter_handle].type;
    grounded_parameters[parameter_handle] =
        problem.constants_by_type[type_handle][constant_index];
  }
  for (size_t i = 0; i < arguments.size(); ++i) {
    if (auto p = std::get_if<ConstantHandle>(&predicate.arguments[i])) {
      if (*p != arguments[i]) {
        return false;
      }
    } else {
      auto parameter_handle = std::get<ParameterHandle>(predicate.arguments[i]);
      // Possible speedup (no insertion needed if subtype does not match)
      auto [it, inserted] =
          grounded_parameters.try_emplace(parameter_handle, arguments[i]);
      if (inserted) {
        // Insertion happend
        if (!is_subtype(problem, problem.constants[it->second].type,
                        action.parameters[parameter_handle].type)) {
          return false;
        }
      } else {
        if (it->second != arguments[i]) {
          return false;
        }
      }
    }
  }
  return true;
}

} // namespace model

#endif /* end of include guard: MODEL_UTILS_HPP */
