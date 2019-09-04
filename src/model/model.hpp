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
  std::string name;
  TypePtr parent;
};

struct Parameter {
  explicit Parameter(const std::string &name, TypePtr type)
      : name{name}, type{type} {}
  std::string name;
  TypePtr type;
};

struct PredicateDefinition {
  explicit PredicateDefinition(const std::string &name) : name{name} {}
  std::string name;
  std::vector<Parameter> parameters;
};

struct Constant {
  Constant(const std::string &name, TypePtr type) : name{name}, type{type} {}
  std::string name;
  TypePtr type;
};

using Argument = std::variant<ConstantPtr, ParameterPtr>;

struct Junction;
struct PredicateEvaluation;
template <bool> struct Trivial {};
using TrivialTrue = Trivial<true>;
using TrivialFalse = Trivial<false>;

using Condition =
    std::variant<Junction, PredicateEvaluation, TrivialTrue, TrivialFalse>;

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
  std::string name;
  std::vector<Parameter> parameters;
  std::vector<PredicateEvaluation> preconditions;
  std::vector<PredicateEvaluation> effects;
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
  std::vector<PredicateEvaluation> goal;
};

struct AbstractAction {
  std::string name;
  std::vector<model::Parameter> parameters;
  model::Condition precondition = TrivialTrue{};
  model::Condition effect = TrivialTrue{};
};

struct AbstractProblem {
  std::string domain_name;
  std::string problem_name;
  std::vector<std::string> requirements;
  std::vector<Type> types;
  std::vector<Constant> constants;
  std::vector<PredicateDefinition> predicates;
  std::vector<AbstractAction> actions;
  model::Condition initial_state = TrivialTrue{};
  model::Condition goal = TrivialTrue{};
};

bool is_subtype(const std::vector<Type> &types, TypePtr type,
                TypePtr supertype) {
  if (type == supertype) {
    return true;
  }
  while (types[type.p].parent != type) {
    type = types[type.p].parent;
    if (type == supertype) {
      return true;
    }
  }
  return false;
}

std::string print_condition(const AbstractProblem &problem,
                            const Condition &condition,
                            const AbstractAction &action) {
  std::string result;
  if (std::holds_alternative<Junction>(condition)) {
    const Junction &junction = std::get<Junction>(condition);
    result += "(";
    for (auto it = junction.arguments.cbegin(); it != junction.arguments.cend();
         ++it) {
      if (it != junction.arguments.cbegin()) {
        result +=
            junction.connective == Junction::Connective::And ? " ∧ " : " ∨ ";
      }
      result += print_condition(problem, *it, action);
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
    for (auto it = predicate.arguments.cbegin();
         it != predicate.arguments.cend(); ++it) {
      if (it != predicate.arguments.cbegin()) {
        result += ", ";
      }
      if (std::holds_alternative<ConstantPtr>(*it)) {
        result += problem.constants[std::get<ConstantPtr>(*it).p].name;
      } else {
        result += "?" + action.parameters[std::get<ParameterPtr>(*it).p].name;
      }
    }
    result += ")";
  }
  return result;
}

std::ostream &operator<<(std::ostream &out, const AbstractProblem &problem) {
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
    for (auto it = predicate.parameters.cbegin();
         it != predicate.parameters.cend(); ++it) {
      if (it != predicate.parameters.cbegin()) {
        out << ", ";
      }
      out << '?' << (*it).name;
      if ((*it).type.p != 0) {
        out << " - " << problem.types[(*it).type.p].name;
      }
    }
    out << ')' << '\n';
  }
  out << "Actions:" << '\n';
  for (auto &action : problem.actions) {
    out << '\t' << action.name << '(';
    for (auto it = action.parameters.cbegin(); it != action.parameters.cend();
         ++it) {
      if (it != action.parameters.cbegin()) {
        out << ", ";
      }
      out << '?' << (*it).name;
      if ((*it).type.p != 0) {
        out << " - " << problem.types[(*it).type.p].name;
      }
    }
    out << ')' << '\n';
    out << '\t' << "Preconditions:" << '\n';
    out << '\t' << '\t'
        << print_condition(problem, action.precondition, action);
    out << '\n';
    out << '\t' << "Effects:" << '\n';
    out << '\t' << '\t' << print_condition(problem, action.effect, action);
    out << '\n';
    out << '\n';
  }
  out << "Initial state: "
      << print_condition(problem, problem.initial_state, AbstractAction{});
  out << '\n';
  out << "Goal: " << print_condition(problem, problem.goal, AbstractAction{});
  out << '\n';
  return out;
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
    for (auto it = predicate.parameters.cbegin();
         it != predicate.parameters.cend(); ++it) {
      out << '?' << (*it).name;
      if ((*it).type.p != 0) {
        out << " - " << problem.types[(*it).type.p].name;
      }
      if (it != predicate.parameters.cend()) {
        out << ", ";
      }
    }
    out << ')' << '\n';
  }
  out << "Actions:" << '\n';
  for (auto &action : problem.actions) {
    out << '\t' << action.name << '(';
    for (auto it = action.parameters.cbegin(); it != action.parameters.cend();
         ++it) {
      if (it != action.parameters.cbegin()) {
        out << ", ";
      }
      out << '?' << (*it).name;
      if ((*it).type.p != 0) {
        out << " - " << problem.types[(*it).type.p].name;
      }
    }
    out << ')' << '\n';
    if (action.preconditions.size() > 0) {
      out << '\t' << "Preconditions:";
      for (const auto &precondition : action.preconditions) {
        out << '\n';
        out << '\t' << '\t';
        if (precondition.negated) {
          out << "¬";
        }
        out << problem.predicates[precondition.definition.p].name;
        out << "(";
        for (auto it = precondition.arguments.cbegin();
             it != precondition.arguments.cend(); ++it) {
          if (it != precondition.arguments.cbegin()) {
            out << ", ";
          }
          if (std::holds_alternative<ConstantPtr>(*it)) {
            out << problem.constants[std::get<ConstantPtr>(*it).p].name;
          } else {
            out << "?" << action.parameters[std::get<ParameterPtr>(*it).p].name;
          }
        }
        out << ")";
      }
      out << '\n';
    }
    if (action.effects.size() > 0) {
      out << '\t' << "Effects:";
      for (const auto &effect : action.effects) {
        out << '\n';
        out << '\t' << '\t';
        if (effect.negated) {
          out << "¬";
        }
        out << problem.predicates[effect.definition.p].name;
        out << "(";
        for (auto it = effect.arguments.cbegin(); it != effect.arguments.cend();
             ++it) {
          if (it != effect.arguments.cbegin()) {
            out << ", ";
          }
          if (std::holds_alternative<ConstantPtr>(*it)) {
            out << problem.constants[std::get<ConstantPtr>(*it).p].name;
          } else {
            out << "?" + action.parameters[std::get<ParameterPtr>(*it).p].name;
          }
        }
        out << ")";
      }
      out << '\n';
    }
    out << '\n';
  }
  out << "Initial state:";
  for (const auto &predicate : problem.initial_state) {
    out << '\n';
    out << '\t';
    if (predicate.negated) {
      out << "¬";
    }
    out << problem.predicates[predicate.definition.p].name;
    out << "(";
    for (auto it = predicate.arguments.cbegin();
         it != predicate.arguments.cend(); ++it) {
      if (it != predicate.arguments.cbegin()) {
        out << ", ";
      }
      if (std::holds_alternative<ConstantPtr>(*it)) {
        out << problem.constants[std::get<ConstantPtr>(*it).p].name;
      }
    }
    out << ")";
  }
  out << '\n';
  out << "Goal:";
  for (const auto &predicate : problem.goal) {
    out << '\n';
    out << '\t';
    if (predicate.negated) {
      out << "¬";
    }
    out << problem.predicates[predicate.definition.p].name;
    out << "(";
    for (auto it = predicate.arguments.cbegin();
         it != predicate.arguments.cend(); ++it) {
      if (it != predicate.arguments.cbegin()) {
        out << ", ";
      }
      if (std::holds_alternative<ConstantPtr>(*it)) {
        out << problem.constants[std::get<ConstantPtr>(*it).p].name;
      }
    }
    out << ")";
  }
  out << '\n';
  return out;
}

} // namespace model

#endif /* end of include guard: MODEL_HPP */
