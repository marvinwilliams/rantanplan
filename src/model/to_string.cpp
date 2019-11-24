#include "model/to_string.hpp"
#include "model/normalized_problem.hpp"
#include "model/problem.hpp"

#include <cassert>
#include <sstream>
#include <string>
#include <variant>

std::string to_string(normalized::TypeHandle type,
                      const normalized::Problem &problem) {
  const auto &t = problem.types[type];
  std::stringstream ss;
  ss << problem.type_names[type];
  if (t.parent != 0) {
    ss << " - " << problem.type_names[t.parent];
  }
  return ss.str();
}

std::string to_string(normalized::ConstantHandle constant,
                      const normalized::Problem &problem) {
  const auto &c = problem.constants[constant];
  std::stringstream ss;
  ss << problem.constant_names[constant];
  if (c.type != 0) {
    ss << " - " << problem.type_names[c.type];
  }
  return ss.str();
}

std::string to_string(normalized::PredicateHandle predicate,
                      const normalized::Problem &problem) {
  const auto &p = problem.predicates[predicate];
  std::stringstream ss;
  ss << problem.predicate_names[predicate] << '(';
  for (auto it = p.parameter_types.cbegin(); it != p.parameter_types.cend();
       ++it) {
    if (it != p.parameter_types.cbegin()) {
      ss << ", ";
    }
    /* ss << '?' << it->name; */
    ss << problem.type_names[*it];
  }
  ss << ')';
  return ss.str();
}

std::string to_string(const normalized::Condition &predicate,
                      const normalized::Action &action,
                      const normalized::Problem &problem) {
  std::stringstream ss;
  if (predicate.negated) {
    ss << '!';
  }
  ss << problem.predicate_names[predicate.definition];
  ss << '(';
  for (auto it = predicate.arguments.cbegin(); it != predicate.arguments.cend();
       ++it) {
    if (it != predicate.arguments.cbegin()) {
      ss << ", ";
    }
    if (it->constant) {
      ss << problem.constant_names[it->index];
    } else {
      assert(!action.parameters[it->index].constant);
      ss << '[' << problem.type_names[action.parameters[it->index].index]
         << ']';
    }
  }
  ss << ')';
  return ss.str();
}

std::string to_string(const normalized::Condition &predicate,
                      const normalized::Problem &problem) {
  std::stringstream ss;
  if (predicate.negated) {
    ss << '!';
  }
  ss << problem.predicate_names[predicate.definition];
  ss << '(';
  for (auto it = predicate.arguments.cbegin(); it != predicate.arguments.cend();
       ++it) {
    if (it != predicate.arguments.cbegin()) {
      ss << ", ";
    }
    assert(it->constant);
    ss << problem.constant_names[it->index];
  }
  ss << ')';
  return ss.str();
}

std::string to_string(const normalized::PredicateInstantiation &predicate,
                      const normalized::Problem &problem) {
  std::stringstream ss;
  ss << problem.predicate_names[predicate.definition];
  ss << '(';
  for (auto it = predicate.arguments.cbegin(); it != predicate.arguments.cend();
       ++it) {
    if (it != predicate.arguments.cbegin()) {
      ss << ", ";
    }
    ss << problem.constant_names[*it];
  }
  ss << ')';
  return ss.str();
}

/* std::string to_string(const normalized::Condition &condition, */
/*                       const normalized::Problem &problem) { */
/*   std::stringstream ss; */
/*   if (std::holds_alternative<Junction>(condition)) { */
/*     const normalized::Junction &junction = std::get<Junction>(condition); */
/*     ss << '('; */
/*     for (auto it = junction.arguments.cbegin(); it !=
 * junction.arguments.cend(); */
/*          ++it) { */
/*       if (it != junction.arguments.cbegin()) { */
/*         ss << (junction.connective == normalized::Junction::Connective::And
 */
/*                    ? " ∧ " */
/*                    : " ∨ "); */
/*       } */
/*       ss << to_string(*it, problem); */
/*     } */
/*     ss << ')'; */
/*   } else { */
/*     const normalized::ConditionPredicate &predicate = */
/*         std::get<ConditionPredicate>(condition); */
/*     ss << to_string(predicate, problem); */
/*   } */
/*   return ss.str(); */
/* } */

std::string to_string(normalized::ActionHandle action,
                      const normalized::Problem &problem) {
  const auto &a = problem.actions[action];
  std::stringstream ss;
  ss << problem.action_names[action] << '(';
  for (auto it = a.parameters.cbegin(); it != a.parameters.cend(); ++it) {
    if (it != a.parameters.cbegin()) {
      ss << ", ";
    }
    if (it->constant) {
      ss << problem.constant_names[it->index];
    } else {
      ss << '[' << problem.type_names[it->index] << ']';
    }
  }
  ss << ')' << '\n';
  ss << '\t' << "Preconditions:" << '\n';
  for (const auto &precondition : a.preconditions) {
    ss << '\t' << '\t' << to_string(precondition, a, problem) << '\n';
  }
  ss << '\t' << "Effects:" << '\n';
  for (const auto &effect : a.effects) {
    ss << '\t' << '\t' << to_string(effect, a, problem) << '\n';
  }
  return ss.str();
}

std::string to_string(const normalized::Problem &problem) {
  std::stringstream ss;
  ss << "Domain: " << problem.domain_name << '\n';
  ss << "Problem: " << problem.problem_name << '\n';
  ss << "Requirements:";
  for (auto &requirement : problem.requirements) {
    ss << ' ' << requirement;
  }
  ss << '\n';
  ss << "Types:" << '\n';
  for (size_t i = 0; i < problem.types.size(); ++i) {
    ss << '\t' << to_string(normalized::TypeHandle{i}, problem) << '\n';
  }
  ss << "Constants:" << '\n';
  for (size_t i = 0; i < problem.constants.size(); ++i) {
    ss << '\t' << to_string(normalized::ConstantHandle{i}, problem) << '\n';
  }
  ss << "Predicates:" << '\n';
  for (size_t i = 0; i < problem.predicates.size(); ++i) {
    ss << '\t' << to_string(normalized::PredicateHandle{i}, problem) << '\n';
  }
  ss << "Actions:" << '\n';
  for (size_t i = 0; i < problem.actions.size(); ++i) {
    ss << '\t' << to_string(normalized::ActionHandle{i}, problem) << '\n';
  }
  ss << "Initial state:" << '\n';
  for (auto &predicate : problem.init) {
    ss << '\t' << to_string(predicate, problem) << '\n';
  }
  ss << '\n';
  ss << "Goal:" << '\n';
  for (auto &[predicate, negated] : problem.goal) {
    ss << '\t' << (negated ? "not " : "") << to_string(predicate, problem)
       << '\n';
  }
  return ss.str();
}
