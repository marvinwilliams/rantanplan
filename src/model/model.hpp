#ifndef MODEL_HPP
#define MODEL_HPP

#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <variant>
#include <vector>

namespace model {

template <typename T> struct Index {
  using value_type = size_t;
  template <typename Integral,
            std::enable_if_t<std::is_integral_v<Integral>, int> = 0>
  Index(Integral i = 0) : i{static_cast<value_type>(i)} {}
  Index(const Index &index) : Index{index.i} {}
  Index() : Index(0) {}
  Index &operator=(const Index<T> &index) {
    i = index.i;
    return *this;
  }

  operator value_type() const { return i; }

  Index &operator++() {
    ++i;
    return *this;
  }

  Index operator++(int) {
    Index old{*this};
    ++(*this);
    return old;
  }

  value_type i;
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
  size_t operator()(const Index<T> &index) const {
    return std::hash<size_t>{}(index.i);
  }
};

struct Type;
struct Constant;
struct Parameter;
struct PredicateDefinition;
struct GroundPredicate;
struct Action;

using TypePtr = Index<Type>;
using ConstantPtr = Index<Constant>;
using ParameterPtr = Index<Parameter>;
using PredicatePtr = Index<PredicateDefinition>;
using GroundPredicatePtr = Index<GroundPredicate>;
using ActionPtr = Index<Action>;

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
  std::optional<ConstantPtr> constant;
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

  PredicateEvaluation(PredicatePtr predicate, std::vector<Argument> arguments)
      : definition{predicate}, arguments{std::move(arguments)} {}
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

inline bool operator==(const Action &first, const Action &second) {
  return first.name == second.name;
}

struct GroundPredicate {
  GroundPredicate(PredicatePtr definition, std::vector<ConstantPtr> arguments)
      : definition{definition}, arguments(std::move(arguments)) {}

  GroundPredicate(const PredicateEvaluation &predicate) {
    /* assert(is_grounded(predicate)); */
    definition = predicate.definition;
    arguments.reserve(predicate.arguments.size());
    for (const auto &argument : predicate.arguments) {
      arguments.push_back(std::get<ConstantPtr>(argument));
    }
  }

  GroundPredicate(const PredicateEvaluation &predicate, const Action &action) {
    /* assert(is_grounded(predicate, action)); */
    definition = predicate.definition;
    arguments.reserve(predicate.arguments.size());
    for (const auto &argument : predicate.arguments) {
      if (const ConstantPtr *p = std::get_if<ConstantPtr>(&argument)) {
        arguments.push_back(*p);
      } else {
        arguments.push_back(
            *(action.parameters[std::get<ParameterPtr>(argument)].constant));
      }
    }
  }

  PredicatePtr definition;
  std::vector<ConstantPtr> arguments;
};

inline bool operator==(const GroundPredicate &first,
                       const GroundPredicate &second) {
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

struct ProblemHeader {
  std::string domain_name;
  std::string problem_name;
  std::vector<std::string> requirements;
};

struct ProblemBase {
  virtual ~ProblemBase() = default;

  std::vector<Type> types;
  std::vector<Constant> constants;
  std::vector<PredicateDefinition> predicates;

protected:
  ProblemBase() = default;
  ProblemBase(const ProblemBase &) = default;
  ProblemBase(ProblemBase &&) = default;
  ProblemBase &operator=(const ProblemBase &) = default;
  ProblemBase &operator=(ProblemBase &&) = default;
};

struct Problem : ProblemBase {
  explicit Problem(ProblemHeader header) : header{std::move(header)} {}
  ProblemHeader header;
  std::vector<Action> actions;
  std::vector<PredicateEvaluation> initial_state;
  std::vector<PredicateEvaluation> goal;
};

struct AbstractAction {
  std::string name;
  std::vector<Parameter> parameters;
  Condition precondition = TrivialTrue{};
  Condition effect = TrivialTrue{};
};

struct AbstractProblem : ProblemBase {
  ProblemHeader header;
  std::vector<AbstractAction> actions;
  model::Condition initial_state = TrivialTrue{};
  model::Condition goal = TrivialTrue{};
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
