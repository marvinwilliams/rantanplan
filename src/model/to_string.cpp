#include "model/to_string.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"

#include <cassert>
#include <iterator>
#include <sstream>
#include <string>
#include <variant>

using namespace normalized;

std::string to_string(const TypeIndex &type, const Problem &problem) {
  std::stringstream ss;
  ss << problem.get_name(type);
  if (problem.types[type].supertype != type) {
    ss << " - " << problem.get_name(problem.types[type].supertype);
  }
  return ss.str();
}

std::string to_string(const ConstantIndex &constant, const Problem &problem) {
  std::stringstream ss;
  ss << problem.get_name(constant);
  if (problem.constants[constant].type != TypeIndex{0}) {
    ss << " - " << problem.get_name(problem.constants[constant].type);
  }
  return ss.str();
}

std::string to_string(const PredicateIndex &predicate, const Problem &problem) {
  std::stringstream ss;
  ss << problem.get_name(predicate) << '(';
  print_list(ss, problem.predicates[predicate].parameter_types.begin(),
             problem.predicates[predicate].parameter_types.end(), ", ",
             [&problem](TypeIndex type) { return problem.get_name(type); });
  ss << ')';
  return ss.str();
}

std::string to_string(const Condition &condition, const Action &action,
                      const Problem &problem) {
  std::stringstream ss;
  ss << (condition.positive ? "" : "!")
     << problem.get_name(condition.atom.predicate) << '(';
  print_list(ss, condition.atom.arguments.begin(),
             condition.atom.arguments.end(), ", ",
             [&problem, &action](Argument a) {
               if (a.is_parameter()) {
                 std::stringstream parameter_ss;
                 parameter_ss << '[';
                 parameter_ss << problem.get_name(
                     action.parameters[a.get_parameter_index()].get_type());
                 parameter_ss << "] #";
                 parameter_ss << a.get_parameter_index();
                 return parameter_ss.str();
               } else {
                 return problem.get_name(a.get_constant());
               }
             });
  ss << ')';
  return ss.str();
}

std::string to_string(const GroundAtom &atom, const Problem &problem) {
  std::stringstream ss;
  ss << problem.get_name(atom.predicate)<< '(';
  print_list(ss, atom.arguments.begin(), atom.arguments.end(), ", ",
             [&problem](ConstantIndex c) { return problem.get_name(c); });
  ss << ')';
  return ss.str();
}

std::string to_string(const Action &action, const Problem &problem) {
  std::stringstream ss;
  ss << problem.action_names[action.id]<< '(';
  print_list(ss, action.parameters.begin(), action.parameters.end(), ", ",
             [&problem](Parameter p) {
               if (p.is_free()) {
                 return '[' + problem.get_name(p.get_type()) + ']';
               } else {
                 return problem.get_name(p.get_constant());
               }
             });
  ss << ')' << '\n';
  ss << '\t' << "Preconditions:" << '\n';
  for (const auto &[precondition, positive] : action.ground_preconditions) {
    ss << '\t' << '\t' << (positive ? "" : "!")
       << to_string(precondition, problem) << '\n';
  }
  for (const auto &precondition : action.preconditions) {
    ss << '\t' << '\t' << to_string(precondition, action, problem) << '\n';
  }
  ss << '\t' << "Effects:" << '\n';
  for (const auto &[effect, positive] : action.ground_effects) {
    ss << '\t' << '\t' << (positive ? "" : "!") << to_string(effect, problem)
       << '\n';
  }
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
  for (size_t i = 0; i < problem.types.size(); ++i) {
    ss << '\t' << to_string(TypeIndex{i}, problem) << '\n';
  }
  ss << "Constants:" << '\n';
  for (size_t i = 0; i < problem.constants.size(); ++i) {
    ss << '\t' << to_string(ConstantIndex{i}, problem) << '\n';
  }
  ss << "Predicates:" << '\n';
  for (size_t i = 0; i < problem.predicates.size(); ++i) {
    ss << '\t' << to_string(PredicateIndex{i}, problem) << '\n';
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
    ss << step << ": " << '('
       << plan.problem->action_names[plan.problem->actions[action].id] << ' ';
    print_list(
        ss, arguments.begin(), arguments.end(), ", ",
        [&plan](ConstantIndex c) { return plan.problem->constant_names[c]; });
    ss << ')' << '\n';
    ++step;
  }
  return ss.str();
}
