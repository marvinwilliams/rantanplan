#ifndef MODEL_HPP
#define MODEL_HPP

#include <optional>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

namespace model {

template <typename T> struct Pointer {
  template <typename Integral>
  Pointer(Integral p) : p{static_cast<size_t>(p)} {}
  size_t p;
};

template <typename T>
bool operator==(const Pointer<T> &first, const Pointer<T> &second) {
  return first.p == second.p;
}

template <typename T>
bool operator!=(const Pointer<T> &first, const Pointer<T> &second) {
  return first.p != second.p;
}

struct Type;
struct Constant;
struct Parameter;
struct PredicateDefinition;

using TypePtr = Pointer<Type>;
using ConstantPtr = Pointer<Constant>;
using ParameterPtr = Pointer<Parameter>;
using PredicatePtr = Pointer<PredicateDefinition>;

struct Type {
  Type(const std::string &name, TypePtr parent) : name{name}, parent{parent} {}
  const std::string name;
  const TypePtr parent;
};

struct Parameter {
  explicit Parameter(const std::string &name, TypePtr type)
      : name{name}, type{type} {}
  const std::string name;
  const TypePtr type;
};

struct PredicateDefinition {
  explicit PredicateDefinition(const std::string &name) : name{name} {}
  const std::string name;
  std::vector<Parameter> parameters;
};

struct Constant {
  Constant(const std::string &name, TypePtr type) : name{name}, type{type} {}
  const std::string name;
  const TypePtr type;
};

using Argument = std::variant<ConstantPtr, ParameterPtr>;

struct Junction;
struct PredicateEvaluation;
using Condition = std::variant<Junction, PredicateEvaluation>;

struct PredicateEvaluation {
  PredicateEvaluation(PredicatePtr predicate) : definition{predicate} {}
  PredicatePtr definition;
  std::vector<Argument> arguments;
  bool negated = false;
};

struct Junction {
  enum class Connective { And, Or };
  Connective connective;
  std::vector<Condition> arguments;
};

struct Action {
  explicit Action(const std::string &name) : name{name} {}
  const std::string name;
  std::vector<Parameter> parameters;
  std::optional<Condition> precondition;
  std::optional<Condition> effect;
};

struct Problem {
  std::string domain_name;
  std::string problem_name;
  std::vector<std::string> requirements;
  std::vector<Type> types;
  std::vector<Constant> constants;
  std::vector<PredicateDefinition> predicates;
  std::vector<Action> actions;
  std::vector<PredicateEvaluation> initial_state;
  Condition goal;
};

bool is_subtype(const Problem &problem, TypePtr type,
                const TypePtr &supertype) {
  if (type == supertype) {
    return true;
  }
  while (problem.types[type.p].parent != type) {
    type = problem.types[type.p].parent;
    if (type == supertype) {
      return true;
    }
  }
  return false;
}

std::string print_condition(const Problem &problem, const Condition &condition,
                            const Action &action) {
  std::string result;
  if (std::holds_alternative<Junction>(condition)) {
    const Junction &junction = std::get<Junction>(condition);
    result += "(";
    for (auto &operand : junction.arguments) {
      result += print_condition(problem, operand, action);
      result +=
          junction.connective == Junction::Connective::And ? " ∧ " : " ∨ ";
    }
    result += ")";
  } else {
    const PredicateEvaluation &predicate =
        std::get<PredicateEvaluation>(condition);
    if (predicate.negated) {
      result += "¬";
    }
    result += problem.predicates[predicate.definition.p].name;
    result += "(";
    for (auto &argument : predicate.arguments) {
      if (std::holds_alternative<ConstantPtr>(argument)) {
        result += problem.constants[std::get<ConstantPtr>(argument).p].name;
      } else {
        result += action.parameters[std::get<ParameterPtr>(argument).p].name;
      }
      result += ", ";
    }
    result += ")";
  }
  return result;
}

std::ostream &operator<<(std::ostream &out, const Problem &problem) {
  out << "Domain: " << problem.domain_name << '\n';
  out << "Problem: " << problem.problem_name << '\n';
  out << "Requirements:";
  for (auto &requirement : problem.requirements) {
    out << ' ' << requirement;
  }
  out << '\n';
  out << "Types:" << '\n';
  for (auto &type : problem.types) {
    out << '\t' << type.name;
    if (type.parent.p != 0) {
      out << " - " << problem.types[type.parent.p].name;
    }
    out << '\n';
  }
  out << "Constants:" << '\n';
  for (auto &constant : problem.constants) {
    out << '\t' << constant.name;
    if (constant.type.p != 0) {
      out << " - " << problem.types[constant.type.p].name;
    }
    out << '\n';
  }
  out << "Predicates:" << '\n';
  for (auto &predicate : problem.predicates) {
    out << '\t' << predicate.name << '(';
    for (auto &parameter : predicate.parameters) {
      out << parameter.name;
      if (parameter.type.p != 0) {
        out << " - " << problem.types[parameter.type.p].name;
      }
      out << ", ";
    }
    out << ')' << '\n';
  }
  out << "Actions:" << '\n';
  for (auto &action : problem.actions) {
    out << '\t' << action.name << '(';
    for (auto &parameter : action.parameters) {
      out << parameter.name;
      if (parameter.type.p != 0) {
        out << " - " << problem.types[parameter.type.p].name;
      }
      out << ", ";
    }
    out << ')' << '\n';
    if (action.precondition) {
      out << '\t' << '\t' << "Precondition: ";
      out << print_condition(problem, *action.precondition, action);
      out << '\n';
    }
    if (action.effect) {
      out << '\t' << '\t' << "Effect: ";
      out << print_condition(problem, *action.effect, action);
      out << '\n';
    }
  }
  out << "Initial state:" << '\n';
  for (auto &init_predicate : problem.initial_state) {
    out << '\t';
    out << problem.predicates[init_predicate.definition.p].name;
    out << '(';
    for (auto &argument : init_predicate.arguments) {
      out << problem.constants[std::get<ConstantPtr>(argument).p].name;
      out << ", ";
    }
    out << ')';
    out << '\n';
  }
  out << "Goal: " << print_condition(problem, problem.goal, Action{"tmp"});
  out << '\n';
  return out;
}

} // namespace model

#endif /* end of include guard: MODEL_HPP */
