#ifndef MODEL_UTILS_HPP
#define MODEL_UTILS_HPP

#include "model/model.hpp"

#include <algorithm>
#include <cassert>
#include <variant>
#include <vector>

namespace model {

class ArgumentMapping {
public:
  ArgumentMapping(const Action &action, const PredicateEvaluation &predicate)
      : action{action} {
    for (size_t i = 0; i < action.parameters.size(); ++i) {
      std::vector<size_t> parameter_matches;
      for (size_t j = 0; j < predicate.arguments.size(); ++j) {
        auto parameter_ptr = std::get_if<ParameterPtr>(&predicate.arguments[j]);
        if (parameter_ptr && *parameter_ptr == i) {
          parameter_matches.push_back(j);
        }
      }
      if (!parameter_matches.empty()) {
        matches_.emplace_back(i, std::move(parameter_matches));
      }
    }
  }

  ArgumentMapping(const ArgumentMapping &other) = default;
  ArgumentMapping(ArgumentMapping &&other) = default;

  size_t size() const { return matches_.size(); }

  size_t get_action_parameter(size_t index) const {
    return matches_[index].first;
  }

  const std::vector<size_t> &get_predicate_parameters(size_t index) const {
    return matches_[index].second;
  }

  const Action &action;

private:
  std::vector<std::pair<size_t, std::vector<size_t>>> matches_;
};

class ArgumentAssignment {
public:
  ArgumentAssignment(const ArgumentMapping &mapping,
                     const std::vector<size_t> &arguments)
      : action{mapping.action} {
    assert(arguments.size() == mapping.size());
    arguments_.reserve(mapping.size());
    for (size_t i = 0; i < mapping.size(); ++i) {
      arguments.emplace_back(mapping.get_action_parameter(i), arguments[i]);
    }
  }

  const Action &action;

private:
  std::vector<std::pair<size_t, size_t>> arguments_;
};

struct ActionPredicateAssignment {
  ActionPredicateAssignment(ArgumentAssignment assignment,
                            GroundPredicate predicate)
      : assignment{std::move(assignment)}, predicate{std::move(predicate)} {}
  ArgumentAssignment assignment;
  GroundPredicate predicate;
};

struct Support {
  std::vector<std::pair<size_t, ArgumentAssignment>> pos_precondition_support_;
  std::vector<std::pair<size_t, ArgumentAssignment>> neg_precondition_support_;
  std::vector<std::pair<size_t, ArgumentAssignment>> pos_effect_support_;
  std::vector<std::pair<size_t, ArgumentAssignment>> neg_effect_support_;
};

bool is_subtype(const std::vector<Type> &types, TypePtr type,
                TypePtr supertype) {
  if (type == supertype) {
    return true;
  }
  while (get(types, type).parent != type) {
    type = get(types, type).parent;
    if (type == supertype) {
      return true;
    }
  }
  return false;
}

bool is_unifiable(const GroundPredicate &grounded_predicate,
                  const PredicateEvaluation &predicate) {
  if (grounded_predicate.definition != predicate.definition) {
    return false;
  }
  return std::mismatch(
             grounded_predicate.arguments.cbegin(),
             grounded_predicate.arguments.cend(), predicate.arguments.cbegin(),
             [](const auto &constant, const auto &argument) {
               const auto const_a = std::get_if<ConstantPtr>(&argument);
               return const_a == nullptr || constant == *const_a;
             })
             .first == grounded_predicate.arguments.cend();
}

bool holds(const State &state, const GroundPredicate &predicate,
           bool negated = false) {
  bool in_state =
      std::any_of(state.predicates.cbegin(), state.predicates.cend(),
                  [&predicate](const auto &state_predicate) {
                    return state_predicate == predicate;
                  });
  return negated != in_state;
}

bool holds(const State &state, const PredicateEvaluation &predicate) {
  bool in_state =
      std::any_of(state.predicates.cbegin(), state.predicates.cend(),
                  [&predicate](const auto &state_predicate) {
                    return is_unifiable(state_predicate, predicate);
                  });
  return predicate.negated != in_state;
}

bool holds(const RelaxedState &state, const GroundPredicate &predicate,
           bool negated) {
  if (state.predicates.find(predicate) == state.predicates.end()) {
    return holds(state.initial_state, predicate, negated);
  }
  return true;
}

std::vector<GroundPredicate> ground_predicate(PredicatePtr predicate_ptr,
                                              const Problem &problem) {
  const PredicateDefinition &predicate = problem.predicates[predicate_ptr];

  std::vector<GroundPredicate> predicates;

  std::vector<size_t> number_arguments;
  number_arguments.reserve(predicate.parameters.size());
  for (const auto &parameter : predicate.parameters) {
    number_arguments.push_back(
        problem.constants_of_type[parameter.type].size());
  }

  auto combinations = algorithm::all_combinations(number_arguments);

  predicates.reserve(combinations.size());
  for (const auto &combination : combinations) {
    std::vector<ConstantPtr> arguments;
    arguments.reserve(combination.size());
    for (size_t i = 0; i < combination.size(); ++i) {
      auto type = predicate.parameters[i].type;
      auto constant = problem.constants_of_type[type][combination[i]];
      arguments.push_back(constant);
    }
    predicates.emplace_back(predicate_ptr, std::move(arguments));
  }
  return predicates;
}

GroundPredicate to_ground_predicate(const PredicateEvaluation &predicate) {
  std::vector<ConstantPtr> arguments;
  arguments.reserve(predicate.arguments.size());
  for (const auto &argument : predicate.arguments) {
    arguments.push_back(std::get<ConstantPtr>(argument));
  }
  return GroundPredicate{predicate.definition, std::move(arguments)};
}

} // namespace model

#endif /* end of include guard: MODEL_UTILS_HPP */
