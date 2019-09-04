#ifndef NORMALIZE_HPP
#define NORMALIZE_HPP

#include "model/model.hpp"

#include <algorithm>
#include <vector>

namespace grounding {

model::Condition normalize_condition(const model::Condition &condition) {
  const auto junction = std::get_if<model::Junction>(&condition);
  if (junction == nullptr) {
    return condition;
  }
  model::Junction new_junction{};
  new_junction.connective = junction->connective;
  std::vector<model::Condition> dnf_children{};
  const bool is_conjunction =
      junction->connective == model::Junction::Connective::And;
  for (const auto &child : junction->arguments) {
    auto normalized_child = normalize_condition(child);
    auto child_junction = std::get_if<model::Junction>(&normalized_child);
    // Flatten nested conditions if possible
    if (child_junction && child_junction->connective == junction->connective) {
      new_junction.arguments.insert(new_junction.arguments.cend(),
                                    child_junction->arguments.begin(),
                                    child_junction->arguments.end());
    } else {
      if (is_conjunction) {
        if (std::holds_alternative<model::TrivialTrue>(normalized_child)) {
          continue;
        } else if (std::holds_alternative<model::TrivialFalse>(
                       normalized_child)) {
          return model::TrivialFalse{};
        }
      } else {
        if (std::holds_alternative<model::TrivialTrue>(normalized_child)) {
          return model::TrivialTrue{};
        } else if (std::holds_alternative<model::TrivialFalse>(
                       normalized_child)) {
          continue;
        }
      }
      new_junction.arguments.push_back(std::move(normalized_child));
    }
  }
  for (auto it = new_junction.arguments.cbegin();
       it != new_junction.arguments.cend(); ++it) {
    const auto first_disjunction = std::get_if<model::Junction>(&*it);
    if (first_disjunction &&
        first_disjunction->connective == model::Junction::Connective::Or) {
      model::Junction disjunction = *first_disjunction;
      new_junction.arguments.erase(it);
      auto new_disjunction = model::Junction{};
      new_disjunction.connective = model::Junction::Connective::Or;
      for (const auto &argument : disjunction.arguments) {
        auto new_conjunction = model::Junction{};
        new_conjunction.connective = model::Junction::Connective::And;
        new_conjunction.arguments = new_junction.arguments;
        new_conjunction.arguments.push_back(argument);
        new_disjunction.arguments.push_back(std::move(new_conjunction));
      }
      return normalize_condition(new_disjunction);
    }
  }
  if (new_junction.arguments.size() == 0) {
    if (is_conjunction) {
      return model::TrivialTrue{};
    } else {
      return model::TrivialFalse{};
    }
  } else if (new_junction.arguments.size() == 1) {
    return new_junction.arguments[0];
  }
  return new_junction;
}

std::vector<model::PredicateEvaluation>
to_list(const model::Condition &condition) {
  std::vector<model::PredicateEvaluation> list;
  const auto junction = std::get_if<model::Junction>(&condition);
  if (junction) {
    for (const auto &argument : junction->arguments) {
      list.push_back(std::get<model::PredicateEvaluation>(argument));
    }
  } else {
    const auto predicate = std::get_if<model::PredicateEvaluation>(&condition);
    if (predicate) {
      list.push_back(*predicate);
    }
  }
  return list;
}

std::vector<model::Action>
normalize_action(const model::AbstractAction &action) {
  std::vector<model::Action> new_actions;

  const auto precondition = normalize_condition(action.precondition);
  if (std::holds_alternative<model::TrivialFalse>(precondition)) {
    return new_actions;
  }

  const auto effects = to_list(normalize_condition(action.effect));

  if (effects.size() == 0) {
    return new_actions;
  }

  const auto junction = std::get_if<model::Junction>(&precondition);
  if (junction && junction->connective == model::Junction::Connective::Or) {
    new_actions.reserve(junction->arguments.size());
    unsigned int counter = 0;
    for (const auto &argument : junction->arguments) {
      std::stringstream new_name;
      new_name << action.name << '[' << counter++ << ']';
      model::Action new_action{new_name.str()};
      new_action.parameters = action.parameters;
      new_action.preconditions = to_list(argument);
      new_action.effects = effects;
      new_actions.push_back(std::move(new_action));
    }
  } else {
    model::Action new_action{action.name};
    new_action.parameters = action.parameters;
    new_action.preconditions = to_list(precondition);
    new_action.effects = effects;
    new_actions.push_back(std::move(new_action));
  }
  return new_actions;
}

model::Problem normalize(const model::AbstractProblem &abstract_problem) {
  model::Problem problem;
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

} // namespace grounding

#endif /* end of include guard: NORMALIZE_HPP */
