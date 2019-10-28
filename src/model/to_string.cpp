#include "model/to_string.hpp"
#include "model/model.hpp"

#include <cassert>
#include <sstream>
#include <string>
#include <variant>

namespace model {

std::string to_string(const Type &type, const ProblemBase &problem) {
  std::stringstream ss;
  ss << type.name;
  if (type.parent != 0) {
    ss << " - " << problem.types[type.parent].name;
  }
  return ss.str();
}

std::string to_string(const Constant &constant, const ProblemBase &problem) {
  std::stringstream ss;
  ss << constant.name;
  if (constant.type != 0) {
    ss << " - " << problem.types[constant.type].name;
  }
  return ss.str();
}

std::string to_string(const PredicateDefinition &predicate,
                      const ProblemBase &problem) {
  std::stringstream ss;
  ss << predicate.name << '(';
  for (auto it = predicate.parameters.cbegin();
       it != predicate.parameters.cend(); ++it) {
    if (it != predicate.parameters.cbegin()) {
      ss << ", ";
    }
    /* ss << '?' << it->name; */
    ss << it->name;
    if (it->type != 0) {
      ss << " - " << problem.types[it->type].name;
    }
  }
  ss << ')';
  return ss.str();
}

std::string to_string(const PredicateEvaluation &predicate,
                      const AbstractAction &action,
                      const ProblemBase &problem) {
  std::stringstream ss;
  if (predicate.negated) {
    ss << '!';
  }
  ss << problem.predicates[predicate.definition].name;
  ss << '(';
  for (auto it = predicate.arguments.cbegin(); it != predicate.arguments.cend();
       ++it) {
    if (it != predicate.arguments.cbegin()) {
      ss << ", ";
    }
    if (std::holds_alternative<ConstantHandle>(*it)) {
      ss << problem.constants[std::get<ConstantHandle>(*it)].name;
    } else {
      /* ss << '?' << action.parameters[std::get<ParameterHandle>(*it)].name; */
      ss << action.parameters[std::get<ParameterHandle>(*it)].name;
    }
  }
  ss << ')';
  return ss.str();
}

std::string to_string(const PredicateEvaluation &predicate,
                      const Action &action, const ProblemBase &problem) {
  std::stringstream ss;
  if (predicate.negated) {
    ss << '!';
  }
  ss << problem.predicates[predicate.definition].name;
  ss << '(';
  for (auto it = predicate.arguments.cbegin(); it != predicate.arguments.cend();
       ++it) {
    if (it != predicate.arguments.cbegin()) {
      ss << ", ";
    }
    if (std::holds_alternative<ConstantHandle>(*it)) {
      ss << problem.constants[std::get<ConstantHandle>(*it)].name;
    } else {
      auto parameter_handle = std::get<ParameterHandle>(*it);
      ss << action.parameters[parameter_handle].name;
    }
  }
  ss << ')';
  return ss.str();
}

std::string to_string(const PredicateEvaluation &predicate,
                      const ActionGrounding &grounding,
                      const Problem &problem) {
  std::stringstream ss;
  if (predicate.negated) {
    ss << '!';
  }
  ss << problem.predicates[predicate.definition].name;
  ss << '(';
  for (auto it = predicate.arguments.cbegin(); it != predicate.arguments.cend();
       ++it) {
    if (it != predicate.arguments.cbegin()) {
      ss << ", ";
    }
    if (std::holds_alternative<ConstantHandle>(*it)) {
      ss << problem.constants[std::get<ConstantHandle>(*it)].name;
    } else {
      auto parameter_handle = std::get<ParameterHandle>(*it);
      /* ss << '?' << action.parameters[std::get<ParameterHandle>(*it)].name; */
      if (grounding.assignment.count(parameter_handle) > 0) {
        TypeHandle type_handle = problem.actions[grounding.action_handle]
                                     .parameters[parameter_handle]
                                     .type;
        ss << problem
                  .constants[problem.constants_by_type[type_handle]
                                                      [grounding.assignment.at(
                                                          parameter_handle)]]
                  .name;
      } else {
        ss << problem.actions[grounding.action_handle]
                  .parameters[parameter_handle]
                  .name;
      }
    }
  }
  ss << ')';
  return ss.str();
}

std::string to_string(const PredicateEvaluation &predicate,
                      const ProblemBase &problem) {
  std::stringstream ss;
  if (predicate.negated) {
    ss << '!';
  }
  ss << problem.predicates[predicate.definition].name;
  ss << '(';
  for (auto it = predicate.arguments.cbegin(); it != predicate.arguments.cend();
       ++it) {
    if (it != predicate.arguments.cbegin()) {
      ss << ", ";
    }
    assert(std::holds_alternative<ConstantHandle>(*it));
    ss << problem.constants[std::get<ConstantHandle>(*it)].name;
  }
  ss << ')';
  return ss.str();
}

std::string to_string(const GroundPredicate &predicate,
                      const ProblemBase &problem) {
  std::stringstream ss;
  ss << problem.predicates[predicate.definition].name;
  ss << '(';
  for (auto it = predicate.arguments.cbegin(); it != predicate.arguments.cend();
       ++it) {
    if (it != predicate.arguments.cbegin()) {
      ss << ", ";
    }
    ss << problem.constants[*it].name;
  }
  ss << ')';
  return ss.str();
}

std::string to_string(const Condition &condition, const AbstractAction &action,
                      const ProblemBase &problem) {
  std::stringstream ss;
  if (std::holds_alternative<Junction>(condition)) {
    const Junction &junction = std::get<Junction>(condition);
    ss << '(';
    for (auto it = junction.arguments.cbegin(); it != junction.arguments.cend();
         ++it) {
      if (it != junction.arguments.cbegin()) {
        ss << (junction.connective == Junction::Connective::And ? " ∧ "
                                                                : " ∨ ");
      }
      ss << to_string(*it, action, problem);
    }
    ss << ')';
  } else {
    assert(std::holds_alternative<PredicateEvaluation>(condition));
    const PredicateEvaluation &predicate =
        std::get<PredicateEvaluation>(condition);
    ss << to_string(predicate, action, problem);
  }
  return ss.str();
}

std::string to_string(const Condition &condition, const ProblemBase &problem) {
  std::stringstream ss;
  if (std::holds_alternative<Junction>(condition)) {
    const Junction &junction = std::get<Junction>(condition);
    ss << '(';
    for (auto it = junction.arguments.cbegin(); it != junction.arguments.cend();
         ++it) {
      if (it != junction.arguments.cbegin()) {
        ss << (junction.connective == Junction::Connective::And ? " ∧ "
                                                                : " ∨ ");
      }
      ss << to_string(*it, problem);
    }
    ss << ')';
  } else {
    const PredicateEvaluation &predicate =
        std::get<PredicateEvaluation>(condition);
    ss << to_string(predicate, problem);
  }
  return ss.str();
}

std::string to_string(const AbstractAction &action,
                      const ProblemBase &problem) {
  std::stringstream ss;
  ss << action.name << '(';
  for (auto it = action.parameters.cbegin(); it != action.parameters.cend();
       ++it) {
    if (it != action.parameters.cbegin()) {
      ss << ", ";
    }
    /* ss << '?' << it->name; */
    ss << it->name;
    if (it->type != 0) {
      ss << " - " << problem.types[it->type].name;
    }
  }
  ss << ')' << '\n';
  if (!std::holds_alternative<TrivialTrue>(action.precondition)) {
    ss << '\t' << "Preconditions:" << '\n';
    ss << '\t' << '\t' << to_string(action.precondition, action, problem);
    ss << '\n';
  }
  if (!std::holds_alternative<TrivialTrue>(action.effect)) {
    ss << '\t' << "Effects:" << '\n';
    ss << '\t' << '\t' << to_string(action.effect, action, problem);
    ss << '\n';
  }
  return ss.str();
}

std::string to_string(const Action &action, const ProblemBase &problem) {
  std::stringstream ss;
  ss << action.name << '(';
  for (auto it = action.parameters.cbegin(); it != action.parameters.cend();
       ++it) {
    if (it != action.parameters.cbegin()) {
      ss << ", ";
    }
    ss << it->name;
    if (it->type != 0) {
      ss << " - " << problem.types[it->type].name;
    }
  }
  ss << ')' << '\n';
  ss << '\t' << "Preconditions:" << '\n';
  for (const auto &precondition : action.preconditions) {
    ss << '\t' << '\t' << to_string(precondition, action, problem) << '\n';
  }
  ss << '\t' << "Effects:" << '\n';
  for (const auto &effect : action.effects) {
    ss << '\t' << '\t' << to_string(effect, action, problem) << '\n';
  }
  return ss.str();
}

std::string to_string(const ActionGrounding &grounding,
                      const Problem &problem) {
  std::stringstream ss;
  const auto &action = problem.actions[grounding.action_handle];
  ss << action.name << '(';
  for (size_t i = 0; i < action.parameters.size(); ++i) {
    if (i > 0) {
      ss << ", ";
    }
    if (grounding.assignment.count(ParameterHandle{i}) > 0) {
      ss << problem
                .constants[problem.constants_by_type[action.parameters[i].type]
                                                    [grounding.assignment.at(
                                                        ParameterHandle{i})]]
                .name;
    } else {
      ss << action.parameters[i].name;
      if (action.parameters[i].type != 0) {
        ss << " - " << problem.types[action.parameters[i].type].name;
      }
    }
  }
  ss << ')' << '\n';
  ss << '\t' << "Preconditions:" << '\n';
  for (const auto &precondition : action.preconditions) {
    ss << '\t' << '\t' << to_string(precondition, grounding, problem) << '\n';
  }
  ss << '\t' << "Effects:" << '\n';
  for (const auto &effect : action.effects) {
    ss << '\t' << '\t' << to_string(effect, grounding, problem) << '\n';
  }
  return ss.str();
}

std::string to_string(const AbstractProblem &problem) {
  std::stringstream ss;
  ss << "Domain: " << problem.header.domain_name << '\n';
  ss << "Problem: " << problem.header.problem_name << '\n';
  ss << "Requirements:";
  for (auto &requirement : problem.header.requirements) {
    ss << ' ' << requirement;
  }
  ss << '\n';
  ss << "Types:" << '\n';
  for (auto &type : problem.types) {
    ss << '\t' << to_string(type, problem) << '\n';
  }
  ss << "Constants:" << '\n';
  for (auto &constant : problem.constants) {
    ss << '\t' << to_string(constant, problem) << '\n';
  }
  ss << "Predicates:" << '\n';
  for (auto &predicate : problem.predicates) {
    ss << '\t' << to_string(predicate, problem) << '\n';
  }
  ss << "Actions:" << '\n';
  for (auto &action : problem.actions) {
    ss << '\t' << to_string(action, problem) << '\n';
  }
  ss << "Initial state: " << to_string(problem.initial_state, problem);
  ss << '\n';
  ss << "Goal: " << to_string(problem.goal, problem);
  return ss.str();
}

std::string to_string(const Problem &problem) {
  std::stringstream ss;
  ss << "Domain: " << problem.header.domain_name << '\n';
  ss << "Problem: " << problem.header.problem_name << '\n';
  ss << "Requirements:";
  for (auto &requirement : problem.header.requirements) {
    ss << ' ' << requirement;
  }
  ss << '\n';
  ss << "Types:" << '\n';
  for (auto &type : problem.types) {
    ss << '\t' << to_string(type, problem) << '\n';
  }
  ss << "Constants:" << '\n';
  for (auto &constant : problem.constants) {
    ss << '\t' << to_string(constant, problem) << '\n';
  }
  ss << "Predicates:" << '\n';
  for (auto &predicate : problem.predicates) {
    ss << '\t' << to_string(predicate, problem) << '\n';
  }
  ss << "Actions:" << '\n';
  for (auto &action : problem.actions) {
    ss << '\t' << to_string(action, problem) << '\n';
  }
  ss << "Groundings:" << '\n';
  for (auto &groundings : problem.action_groundings) {
    ss << '\t' << to_string(groundings, problem) << '\n';
  }
  ss << "Initial state:" << '\n';
  for (auto &predicate : problem.initial_state) {
    ss << '\t' << to_string(predicate, problem) << '\n';
  }
  ss << '\n';
  ss << "Goal:" << '\n';
  for (auto &predicate : problem.goal) {
    ss << '\t' << to_string(predicate, problem) << '\n';
  }
  return ss.str();
}

} // namespace model
