#ifndef MODEL_UTILS_HPP
#define MODEL_UTILS_HPP

#include "model/model.hpp"
#include "util/combinatorics.hpp"

#include <algorithm>
#include <cassert>
#include <variant>
#include <vector>

namespace model {

inline bool is_subtype(const std::vector<Type> &types, TypeHandle type,
                       TypeHandle supertype) {
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

inline bool is_grounded(const PredicateEvaluation &predicate) noexcept {
  return std::all_of(
      predicate.arguments.cbegin(), predicate.arguments.cend(),
      [](const auto &a) { return std::holds_alternative<ConstantHandle>(a); });
}

inline bool is_grounded(const PredicateEvaluation &predicate,
                        const Action &action) noexcept {
  return std::all_of(
      predicate.arguments.cbegin(), predicate.arguments.cend(),
      [&action](const auto &a) {
        return std::holds_alternative<ConstantHandle>(a) ||
               action.parameters[std::get<ParameterHandle>(a)].constant;
      });
}

inline bool is_grounded(const Action &action) {
  return std::all_of(action.parameters.cbegin(), action.parameters.cend(),
                     [](const auto &p) { return p.constant.has_value(); });
}

struct ArgumentMapping {
  explicit ArgumentMapping(
      const std::vector<model::Parameter> &parameters,
      const std::vector<model::Argument> &arguments) noexcept {
    std::vector<std::vector<ParameterHandle>> parameter_matches{
        parameters.size()};
    for (size_t i = 0; i < arguments.size(); ++i) {
      auto parameter_handle = std::get_if<ParameterHandle>(&arguments[i]);
      if (parameter_handle && !parameters[*parameter_handle].constant) {
        parameter_matches[*parameter_handle].push_back(ParameterHandle{i});
      }
    }
    for (size_t i = 0; i < parameters.size(); ++i) {
      if (!parameter_matches[i].empty()) {
        matches.emplace_back(ParameterHandle{i},
                             std::move(parameter_matches[i]));
      }
    }
  }

  std::vector<std::pair<ParameterHandle, std::vector<ParameterHandle>>> matches;
};

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

struct ArgumentAssignment {
  explicit ArgumentAssignment() noexcept = default;
  explicit ArgumentAssignment(const ArgumentMapping &mapping,
                              const std::vector<size_t> &arguments) noexcept {
    assert(arguments.size() == mapping.size());
    this->arguments.reserve(mapping.matches.size());
    for (size_t i = 0; i < mapping.matches.size(); ++i) {
      this->arguments.insert({mapping.matches[i].first, arguments[i]});
    }
  }

  std::unordered_map<ParameterHandle, size_t, hash::Handle<Parameter>>
      arguments;
};

template <typename Function>
void for_grounded_predicate(const Problem &problem, ActionHandle action_handle, const ArgumentAssignment& assignment,
                            const PredicateEvaluation &predicate, Function f) {
  const Action &action = problem.actions[action_handle];
  ArgumentMapping mapping{action.parameters, predicate.arguments};

  std::vector<ConstantHandle> arguments(predicate.arguments.size());
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    const auto &argument = predicate.arguments[i];
    if (auto constant_handle = std::get_if<model::ConstantHandle>(&argument)) {
      arguments[i] = *constant_handle;
    } else {
      auto parameter_handle = std::get<model::ParameterHandle>(argument);
      if (action.parameters[parameter_handle].constant) {
        arguments[i] = *action.parameters[parameter_handle].constant;
      }
    }
  }

  std::vector<size_t> argument_sizes;
  argument_sizes.reserve(mapping.matches.size());
  std::transform(
      mapping.matches.begin(), mapping.matches.end(),
      std::back_inserter(argument_sizes), [&problem, &action](const auto &m) {
        return get_constants_of_type(action.parameters[m.first].type).size();
      });

  auto combination_iterator = CombinationIterator{argument_sizes};

  model::GroundPredicate ground_predicate{predicate.definition,
                                          std::move(arguments)};

  while (!combination_iterator.end()) {
    const auto &combination = *combination_iterator;
    for (size_t i = 0; i < mapping.matches.size(); ++i) {
      const auto &[parameter_pos, predicate_pos] = mapping.matches[i];
      auto type = action.parameters[parameter_pos].type;
      for (auto index : predicate_pos) {
        ground_predicate.arguments[index] =
            problem.constants_by_type[type][combination[i]];
      }
    }
    f(ground_predicate, ArgumentAssignment{mapping, combination});
    ++combination_iterator;
  }
}

inline bool is_unifiable(const ArgumentAssignment &first,
                         const ArgumentAssignment &second) noexcept {
  return std::all_of(first.arguments.begin(), first.arguments.end(),
                     [&second](auto a) {
                       return (second.arguments.count(a.first) == 0 ||
                               second.arguments.at(a.first) == a.second);
                     });
}

} // namespace model

#endif /* end of include guard: MODEL_UTILS_HPP */
