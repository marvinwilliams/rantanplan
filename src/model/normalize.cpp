#include "model/normalize.hpp"
#include "logging/logging.hpp"
#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "model/to_string.hpp"

#include <algorithm>
#include <sstream>
#include <variant>
#include <vector>

namespace model {

logging::Logger normalize_logger{"Normalize"};

Condition normalize_condition(const Condition &condition) noexcept {
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
  if (new_junction.arguments.empty()) {
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

std::vector<PredicateEvaluation> to_list(const Condition &condition) noexcept {
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

std::vector<Action> normalize_action(const AbstractAction &action) noexcept {
  std::vector<Action> new_actions;

  const auto precondition = normalize_condition(action.precondition);
  if (std::holds_alternative<TrivialFalse>(precondition)) {
    return new_actions;
  }

  const auto effects = to_list(normalize_condition(action.effect));

  if (effects.empty()) {
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
    std::transform(action.parameters.cbegin(), action.parameters.cend(),
                   std::back_inserter(new_action.parameters),
                   [](const auto &p) { return p; });
    new_action.preconditions = to_list(precondition);
    new_action.effects = effects;
    new_actions.push_back(std::move(new_action));
  }
  return new_actions;
}

Problem normalize(const AbstractProblem &abstract_problem) noexcept {
  Problem problem{abstract_problem};

  for (auto &action : abstract_problem.actions) {
    LOG_DEBUG(normalize_logger, "Normalizing action \'%s\'...",
              action.name.c_str());
    auto new_actions = normalize_action(action);
    LOG_DEBUG(normalize_logger, "resulted in %u STRIPS actions",
              new_actions.size());
    problem.actions.insert(problem.actions.cend(), new_actions.begin(),
                           new_actions.end());
  }

  std::vector<GroundPredicate> negated_init;
  for (const auto &predicate : to_list(abstract_problem.init)) {
    GroundPredicate ground_predicate{predicate};
    if (predicate.negated) {
      if (std::find(negated_init.begin(), negated_init.end(),
                    ground_predicate) != negated_init.end()) {
        LOG_WARN(normalize_logger,
                 "Found duplicate negated init predicate '%s'",
                 to_string(ground_predicate, problem).c_str());
      } else {
        negated_init.push_back(GroundPredicate{predicate});
      }
    } else {
      if (std::find(problem.init.begin(), problem.init.end(),
                    ground_predicate) != problem.init.end()) {
        LOG_WARN(normalize_logger, "Found duplicate init predicate '%s'",
                 to_string(ground_predicate, problem).c_str());
      } else {
        problem.init.push_back(GroundPredicate{predicate});
      }
    }
  }
  for (const auto &predicate : negated_init) {
    if (std::find(problem.init.begin(), problem.init.end(), predicate) !=
        problem.init.end()) {
      LOG_ERROR(normalize_logger, "Found contradicting init predicate '%s'",
                to_string(predicate, problem).c_str());
    }
  }

  // Reserve space for initial and equality predicates
  problem.init.reserve(problem.init.size() + problem.constants.size());
  for (size_t i = 0; i < problem.constants.size(); ++i) {
    problem.init.push_back(GroundPredicate{
        PredicateHandle{0}, {ConstantHandle{i}, ConstantHandle{i}}});
  }

  for (const auto &predicate : to_list(abstract_problem.goal)) {
    problem.goal.push_back({GroundPredicate{predicate}, predicate.negated});
  }
  return problem;
}

} // namespace model
