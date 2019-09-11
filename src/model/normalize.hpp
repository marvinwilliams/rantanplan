#ifndef NORMALIZE_HPP
#define NORMALIZE_HPP

#include "model/model.hpp"

#include <algorithm>
#include <sstream>
#include <variant>
#include <vector>

namespace model {

Condition normalize_condition(const Condition &condition) {
  const auto junction = std::get_if<Junction>(&condition);
  if (junction == nullptr) {
    return condition;
  }
  Junction new_junction{};
  new_junction.connective = junction->connective;
  std::vector<Condition> dnf_children{};
  const bool is_conjunction = junction->connective == Junction::Connective::And;
  for (const auto &child : junction->arguments) {
    auto normalized_child = normalize_condition(child);
    auto child_junction = std::get_if<Junction>(&normalized_child);
    // Flatten nested conditions if possible
    if (child_junction && child_junction->connective == junction->connective) {
      new_junction.arguments.insert(new_junction.arguments.cend(),
                                    child_junction->arguments.begin(),
                                    child_junction->arguments.end());
    } else {
      if (is_conjunction) {
        if (std::holds_alternative<TrivialTrue>(normalized_child)) {
          continue;
        } else if (std::holds_alternative<TrivialFalse>(normalized_child)) {
          return TrivialFalse{};
        }
      } else {
        if (std::holds_alternative<TrivialTrue>(normalized_child)) {
          return TrivialTrue{};
        } else if (std::holds_alternative<TrivialFalse>(normalized_child)) {
          continue;
        }
      }
      new_junction.arguments.push_back(std::move(normalized_child));
    }
  }
  for (auto it = new_junction.arguments.cbegin();
       it != new_junction.arguments.cend(); ++it) {
    const auto first_disjunction = std::get_if<Junction>(&*it);
    if (first_disjunction &&
        first_disjunction->connective == Junction::Connective::Or) {
      Junction disjunction = *first_disjunction;
      new_junction.arguments.erase(it);
      auto new_disjunction = Junction{};
      new_disjunction.connective = Junction::Connective::Or;
      for (const auto &argument : disjunction.arguments) {
        auto new_conjunction = Junction{};
        new_conjunction.connective = Junction::Connective::And;
        new_conjunction.arguments = new_junction.arguments;
        new_conjunction.arguments.push_back(argument);
        new_disjunction.arguments.push_back(std::move(new_conjunction));
      }
      return normalize_condition(new_disjunction);
    }
  }
  if (new_junction.arguments.size() == 0) {
    if (is_conjunction) {
      return TrivialTrue{};
    } else {
      return TrivialFalse{};
    }
  } else if (new_junction.arguments.size() == 1) {
    return new_junction.arguments[0];
  }
  return new_junction;
}

std::vector<PredicateEvaluation> to_list(const Condition &condition) {
  std::vector<PredicateEvaluation> list;
  const auto junction = std::get_if<Junction>(&condition);
  if (junction) {
    for (const auto &argument : junction->arguments) {
      list.push_back(std::get<PredicateEvaluation>(argument));
    }
  } else {
    const auto predicate = std::get_if<PredicateEvaluation>(&condition);
    if (predicate) {
      list.push_back(*predicate);
    }
  }
  return list;
}

std::vector<Action> normalize_action(const AbstractAction &action) {
  std::vector<Action> new_actions;

  const auto precondition = normalize_condition(action.precondition);
  if (std::holds_alternative<TrivialFalse>(precondition)) {
    return new_actions;
  }

  const auto effects = to_list(normalize_condition(action.effect));

  if (effects.size() == 0) {
    return new_actions;
  }

  const auto junction = std::get_if<Junction>(&precondition);
  if (junction && junction->connective == Junction::Connective::Or) {
    new_actions.reserve(junction->arguments.size());
    unsigned int counter = 0;
    for (const auto &argument : junction->arguments) {
      std::stringstream new_name;
      new_name << action.name << '[' << counter++ << ']';
      Action new_action{new_name.str()};
      new_action.parameters = action.parameters;
      new_action.preconditions = to_list(argument);
      new_action.effects = effects;
      new_actions.push_back(std::move(new_action));
    }
  } else {
    Action new_action{action.name};
    new_action.parameters = action.parameters;
    new_action.preconditions = to_list(precondition);
    new_action.effects = effects;
    new_actions.push_back(std::move(new_action));
  }
  return new_actions;
}

std::vector<std::vector<ConstantPtr>>
normalize_constants(const std::vector<Type> &types,
                    const std::vector<Constant> &constants) {
  std::vector<std::vector<ConstantPtr>> normalized_constants(types.size());
  for (unsigned int i = 0; i < constants.size(); ++i) {
    ConstantPtr constant{i};
    TypePtr type = constants[i].type;
    normalized_constants[type.i].push_back(constant);
    while (get(types, type).parent != type) {
      type = get(types, type).parent;
      normalized_constants[type.i].push_back(constant);
    }
  }
  return normalized_constants;
}

Problem normalize(const AbstractProblem &abstract_problem) {
  Problem problem;
  problem.domain_name = abstract_problem.domain_name;
  problem.problem_name = abstract_problem.problem_name;
  problem.requirements = abstract_problem.requirements;
  problem.types = abstract_problem.types;
  problem.constants = abstract_problem.constants;
  problem.predicates = abstract_problem.predicates;
  for (auto &action : abstract_problem.actions) {
    auto new_actions = normalize_action(action);
    problem.actions.insert(problem.actions.cend(), new_actions.begin(),
                           new_actions.end());
  }
  problem.initial_state = to_list(abstract_problem.initial_state);
  problem.goal = to_list(abstract_problem.goal);
  return problem;
}

} // namespace model

#endif /* end of include guard: NORMALIZE_HPP */
