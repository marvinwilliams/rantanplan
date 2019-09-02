#ifndef MODEL_HPP
#define MODEL_HPP

#include <optional>
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

struct And;
struct Or;

struct PredicateEvaluation {
  PredicateEvaluation(PredicatePtr predicate) : definition{predicate} {}
  PredicatePtr definition;
  std::vector<Argument> arguments;
  bool negated = false;
};

using Condition = std::variant<And, Or, PredicateEvaluation>;

struct And {
  std::vector<Condition> arguments;
  bool negated = false;
};

struct Or {
  std::vector<Condition> arguments;
  bool negated = false;
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

} // namespace model

#endif /* end of include guard: MODEL_HPP */
