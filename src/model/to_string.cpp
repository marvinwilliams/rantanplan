#include "model/to_string.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"

#include <cassert>
#include <iterator>
#include <sstream>
#include <string>
#include <variant>

using namespace normalized;

std::string to_string(const Type &type, const Problem &problem) {
  std::stringstream ss;
  ss << problem.get_name(type.id);
  if (type.supertype != type.id) {
    ss << " - " << problem.get_name(type.supertype);
  }
  return ss.str();
}

std::string to_string(const Constant &constant, const Problem &problem) {
  std::stringstream ss;
  ss << problem.get_name(constant.id);
  if (constant.type.id != TypeIndex{0}) {
    ss << " - " << problem.get_name(constant.type.id);
  }
  return ss.str();
}

std::string to_string(const Predicate &predicate, const Problem &problem) {
  std::stringstream ss;
  ss << problem.get_name(predicate.id) << '(';
  std::transform(predicate.parameter_types.begin(),
                 predicate.parameter_types.end(),
                 std::ostream_iterator<std::string>(ss, ", "),
                 [&problem](Type type) { return problem.get_name(type.id); });
  ss << ')';
  return ss.str();
}

std::string to_string(const Condition &condition, const Action &action,
                      const Problem &problem) {
  std::stringstream ss;
  if (!condition.positive) {
    ss << '!';
  }
  ss << problem.get_name(condition.atom.predicate);
  ss << '(';
  std::transform(
      condition.atom.arguments.begin(), condition.atom.arguments.end(),
      std::ostream_iterator<std::string>(ss, ", "),
      [&problem, &action](Argument a) {
        if (a.is_parameter()) {
          std::stringstream parameter_ss;
          parameter_ss << '[';
          parameter_ss << problem.get_name(
              action.parameters[a.get_parameter_index()].get_type().id);
          parameter_ss << "] #";
          parameter_ss << a.get_parameter_index();
          return parameter_ss.str();
        } else {
          return problem.get_name(a.get_constant().id);
        }
      });
  ss << ')';
  return ss.str();
}

std::string to_string(const GroundAtom &atom, const Problem &problem) {
  std::stringstream ss;
  ss << problem.get_name(atom.predicate);
  ss << '(';
  std::transform(atom.arguments.begin(), atom.arguments.end(),
                 std::ostream_iterator<std::string>(ss, ", "),
                 [&problem](Constant c) { return problem.get_name(c.id); });
  ss << ')';
  return ss.str();
}

std::string to_string(const Action &action, const Problem &problem) {
  std::stringstream ss;
  ss << problem.action_names[action.id] << '(';
  std::transform(action.parameters.begin(), action.parameters.end(),
                 std::ostream_iterator<std::string>(ss, ", "),
                 [&problem](Parameter p) {
                   if (p.is_free()) {
                     return '[' + problem.get_name(p.get_type().id) + ']';
                   } else {
                     return problem.get_name(p.get_constant().id);
                   }
                 });
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
  for (Type t : problem.types) {
    ss << '\t' << to_string(t, problem) << '\n';
  }
  ss << "Constants:" << '\n';
  for (Constant c : problem.constants) {
    ss << '\t' << to_string(c, problem) << '\n';
  }
  ss << "Predicates:" << '\n';
  for (const Predicate &p : problem.predicates) {
    ss << '\t' << to_string(p, problem) << '\n';
  }
  ss << "Actions:" << '\n';
  for (const Action &a : problem.actions) {
    ss << '\t' << to_string(a, problem) << '\n';
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

std::string to_string(const Plan &plan) noexcept {
  std::stringstream ss;
  unsigned int step = 0;
  for (auto [action, arguments] : plan.sequence) {
    ss << step << ": " << '(' << plan.problem->action_names[action] << ' ';
    std::transform(
        arguments.begin(), arguments.end(),
        std::ostream_iterator<std::string>(ss, ", "),
        [&plan](Constant c) { return plan.problem->constant_names[c.id]; });
    ss << ')' << '\n';
    ++step;
  }
  return ss.str();
}
