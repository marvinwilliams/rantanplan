#include "model/to_string.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"

#include <cassert>
#include <sstream>
#include <string>
#include <variant>

using namespace normalized;

std::string to_string(TypeIndex type, const Problem &problem) {
  std::stringstream ss;
  ss << problem.get_name(type);
  if (problem.get(type).supertype != type) {
    ss << " - " << problem.get_name(problem.get(type).supertype);
  }
  return ss.str();
}

std::string to_string(ConstantIndex constant, const Problem &problem) {
  std::stringstream ss;
  ss << problem.get_name(constant);
  if (problem.constants[constant].type != TypeIndex{0}) {
    ss << " - " << problem.get_name(problem.get(constant).type);
  }
  return ss.str();
}

std::string to_string(PredicateIndex predicate, const Problem &problem) {
  std::stringstream ss;
  ss << problem.get_name(predicate) << '(';
  for (auto it = problem.get(predicate).parameter_types.cbegin();
       it != problem.get(predicate).parameter_types.cend(); ++it) {
    if (it != problem.get(predicate).parameter_types.cbegin()) {
      ss << ", ";
    }
    ss << problem.get_name(*it);
  }
  ss << ')';
  return ss.str();
}

std::string to_string(const Condition &condition, const Action &action,
                      const Problem &problem) {
  std::stringstream ss;
  if (!condition.positive) {
    ss << '!';
  }
  ss << problem.get_name(condition.definition);
  ss << '(';
  for (auto it = condition.arguments.cbegin(); it != condition.arguments.cend();
       ++it) {
    if (it != condition.arguments.cbegin()) {
      ss << ", ";
    }
    if (it->is_constant()) {
      ss << problem.get_name(it->get_constant());
    } else {
      ss << '[' << problem.get_name(action.get(it->get_parameter()).get_type())
         << "] #" << it->get_parameter();
    }
  }
  ss << ')';
  return ss.str();
}

std::string to_string(const Condition &condition, const Problem &problem) {
  std::stringstream ss;
  if (!condition.positive) {
    ss << '!';
  }
  ss << problem.get_name(condition.definition);
  ss << '(';
  for (auto it = condition.arguments.cbegin(); it != condition.arguments.cend();
       ++it) {
    if (it != condition.arguments.cbegin()) {
      ss << ", ";
    }
    ss << problem.get_name(it->get_constant());
  }
  ss << ')';
  return ss.str();
}

std::string to_string(const PredicateInstantiation &predicate,
                      const Problem &problem) {
  std::stringstream ss;
  ss << problem.get_name(predicate.definition);
  ss << '(';
  for (auto it = predicate.arguments.cbegin(); it != predicate.arguments.cend();
       ++it) {
    if (it != predicate.arguments.cbegin()) {
      ss << ", ";
    }
    ss << problem.get_name(*it);
  }
  ss << ')';
  return ss.str();
}

std::string to_string(const Action &action, const Problem &problem) {
  std::stringstream ss;
  ss << problem.action_names[get_index(&action, problem)] << '(';
  for (auto it = action.parameters.cbegin(); it != action.parameters.cend();
       ++it) {
    if (it != action.parameters.cbegin()) {
      ss << ", ";
    }
    if (it->is_constant()) {
      ss << problem.get_name(it->get_constant());
    } else {
      ss << '[' << problem.get_name(it->get_type()) << ']';
    }
  }
  ss << ')' << '\n';
  ss << '\t' << "Preconditions:" << '\n';
  for (const auto &precondition : action.preconditions) {
    ss << '\t' << '\t' << to_string(precondition, action, problem) << '\n';
  }
  for (const auto &[precondition, positive] : action.pre_instantiated) {
    ss << '\t' << '\t' << (positive ? "" : "!")
       << to_string(precondition, problem) << '\n';
  }
  ss << '\t' << "Effects:" << '\n';
  for (const auto &effect : action.effects) {
    ss << '\t' << '\t' << to_string(effect, action, problem) << '\n';
  }
  for (const auto &[effect, positive] : action.eff_instantiated) {
    ss << '\t' << '\t' << (positive ? "" : "!") << to_string(effect, problem)
       << '\n';
  }
  return ss.str();
}

std::string to_string(const Problem &problem) {
  std::stringstream ss;
  ss << "Domain: " << problem.domain_name << '\n';
  ss << "Problem: " << problem.problem_name << '\n';
  ss << "Requirements:";
  for (auto &requirement : problem.requirements) {
    ss << ' ' << requirement;
  }
  ss << '\n';
  ss << "Types:" << '\n';
  for (const auto &type : problem.types) {
    ss << '\t' << to_string(type, problem) << '\n';
  }
  ss << "Constants:" << '\n';
  for (const auto &constant : problem.constants) {
    ss << '\t' << to_string(constant, problem) << '\n';
  }
  ss << "Predicates:" << '\n';
  for (const auto &predicate : problem.predicates) {
    ss << '\t' << to_string(predicate, problem) << '\n';
  }
  ss << "Actions:" << '\n';
  for (const auto &action : problem.actions) {
    ss << '\t' << to_string(action, problem) << '\n';
  }
  ss << "Initial state:" << '\n';
  for (auto &predicate : problem.init) {
    ss << '\t' << to_string(predicate, problem) << '\n';
  }
  ss << '\n';
  ss << "Goal:" << '\n';
  for (auto &[predicate, positive] : problem.goal) {
    ss << '\t' << (positive ? "" : "not ") << to_string(predicate, problem)
       << '\n';
  }
  return ss.str();
}
