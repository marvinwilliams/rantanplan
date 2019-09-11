#ifndef MODEL_HPP
#define MODEL_HPP

#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

namespace model {

template <typename T> struct Index {
  template <typename Integral>
  Index(Integral i = 0) : i{static_cast<size_t>(i)} {}
  Index(const Index &index) : Index{index.i} {}
  Index() : Index(0) {}

  Index &operator=(const Index &index) {
    i = index.i;
    return *this;
  }

  size_t i;
};

template <typename T>
inline bool operator==(const Index<T> &first, const Index<T> &second) {
  return first.i == second.i;
}

template <typename T>
inline bool operator!=(const Index<T> &first, const Index<T> &second) {
  return !(first == second);
}

template <typename T> struct IndexHash {
  size_t operator()(const Index<T> &index) {
    return std::hash<size_t>{}(index.i);
  }
};

struct Type;
struct Constant;
struct Parameter;
struct PredicateDefinition;

using TypePtr = Index<Type>;
using ConstantPtr = Index<Constant>;
using ParameterPtr = Index<Parameter>;
using PredicatePtr = Index<PredicateDefinition>;

template <typename T> const T get(const std::vector<T> &list, Index<T> index) {
  return list[index.i];
}

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

struct GroundPredicate {
  PredicatePtr definition;
  std::vector<ConstantPtr> arguments;
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
  std::vector<std::vector<ConstantPtr>> constants_of_type;
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

bool operator==(const GroundPredicate &first, const GroundPredicate &second) {
  return first.definition == second.definition &&
         first.arguments == second.arguments;
}

struct GroundPredicateHash {
  size_t operator()(const GroundPredicate &predicate) const noexcept {
    size_t hash = IndexHash<PredicateDefinition>{}(predicate.definition);
    for (ConstantPtr p : predicate.arguments) {
      hash ^= IndexHash<Constant>{}(p);
    }
    return hash;
  }
};

struct State {
  std::vector<GroundPredicate> predicates;
};

// A relaxed state holds the initial state as well as each predicate which is
// either present or not
struct RelaxedState {
  std::unordered_set<GroundPredicate, GroundPredicateHash> predicates;
  State initial_state;
};

} // namespace model

#endif /* end of include guard: MODEL_HPP */
