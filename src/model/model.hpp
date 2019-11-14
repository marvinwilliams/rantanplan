#ifndef MODEL_HPP
#define MODEL_HPP

#include <cassert>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace model {

template <typename T> struct Handle {
  using value_type = size_t;
  template <typename Integral,
            std::enable_if_t<std::is_integral_v<Integral>, int> = 0>
  explicit Handle(Integral i = 0) : i{static_cast<value_type>(i)} {}
  Handle(const Handle &handle) : Handle{handle.i} {}
  Handle() : Handle(0) {}
  Handle &operator=(const Handle<T> &handle) {
    i = handle.i;
    return *this;
  }

  inline operator value_type() const { return i; }

  inline Handle &operator++() {
    ++i;
    return *this;
  }

  inline Handle operator++(int) {
    Handle old{*this};
    ++(*this);
    return old;
  }

  value_type i;
};

template <typename T>
inline bool operator==(const Handle<T> &first, const Handle<T> &second) {
  return first.i == second.i;
}

template <typename T>
inline bool operator!=(const Handle<T> &first, const Handle<T> &second) {
  return !(first == second);
}

namespace hash {

template <typename T> struct Handle {
  size_t operator()(const ::model::Handle<T> &handle) const {
    return std::hash<size_t>{}(handle.i);
  }
};

} // namespace hash

struct Type;
using TypeHandle = Handle<Type>;

struct Type {
  TypeHandle parent;
};

struct Predicate;
using PredicateHandle = Handle<Predicate>;

struct Predicate {
  std::vector<TypeHandle> parameter_types;
};

struct Constant;
using ConstantHandle = Handle<Constant>;

struct Constant {
  TypeHandle type;
};

struct Parameter {
  bool constant;
  size_t index;
};

using ParameterHandle = Handle<Parameter>;

struct Junction;

struct ConditionPredicate {
  PredicateHandle definition;
  std::vector<Parameter> arguments;
  bool negated = false;
};

template <bool> struct Trivial {};
using TrivialTrue = Trivial<true>;
using TrivialFalse = Trivial<false>;

using Condition =
    std::variant<Junction, ConditionPredicate, TrivialTrue, TrivialFalse>;

struct Junction {
  enum class Connective { And, Or };
  Connective connective;
  std::vector<Condition> arguments;
};

struct PredicateInstantiation;
using PredicateInstantiationHandle = Handle<PredicateInstantiation>;

struct PredicateInstantiation {
  explicit PredicateInstantiation(PredicateHandle definition,
                                  std::vector<ConstantHandle> arguments)
      : definition{definition}, arguments(std::move(arguments)) {}

  PredicateHandle definition;
  std::vector<ConstantHandle> arguments;
};

inline bool operator==(const PredicateInstantiation &first,
                       const PredicateInstantiation &second) {
  return first.definition == second.definition &&
         first.arguments == second.arguments;
}

namespace hash {

struct PredicateInstantiation {
  size_t operator()(const ::model::PredicateInstantiation &predicate) const
      noexcept {
    size_t hash = Handle<Predicate>{}(predicate.definition);
    for (ConstantHandle p : predicate.arguments) {
      hash ^= Handle<Constant>{}(p);
    }
    return hash;
  }
};

} // namespace hash

struct Action;
using ActionHandle = Handle<Action>;

struct Action {
  std::vector<Parameter> parameters;
  std::vector<ConditionPredicate> preconditions;
  std::vector<ConditionPredicate> effects;
  std::vector<std::pair<PredicateInstantiation, bool>> pre_instantiated;
  std::vector<std::pair<PredicateInstantiation, bool>> eff_instantiated;
};

struct AbstractAction {
  std::vector<Parameter> parameters;
  Condition precondition = TrivialTrue{};
  Condition effect = TrivialTrue{};
};

struct ProblemHeader {
  std::string domain_name;
  std::string problem_name;
  std::vector<std::string> requirements;
};

struct ProblemBase {
  virtual ~ProblemBase() = default;

  ProblemHeader header;
  std::vector<Type> types;
  std::vector<std::string> type_names;
  std::vector<Constant> constants;
  std::vector<std::string> constant_names;
  std::vector<Predicate> predicates;
  std::vector<std::string> predicate_names;

protected:
  ProblemBase() = default;
  ProblemBase(const ProblemBase &) = default;
  ProblemBase(ProblemBase &&) = default;
  ProblemBase &operator=(const ProblemBase &) = default;
  ProblemBase &operator=(ProblemBase &&) = default;
};

struct AbstractProblem : ProblemBase {
  std::vector<AbstractAction> actions;
  std::vector<std::string> action_names;
  model::Condition init = TrivialTrue{};
  model::Condition goal = TrivialTrue{};
};

struct Problem : ProblemBase {
  explicit Problem(const ProblemBase &other) : ProblemBase{other} {}

  std::vector<Action> actions;
  std::vector<std::string> action_names;
  std::vector<PredicateInstantiation> init;
  std::vector<std::pair<PredicateInstantiation, bool>> goal;
};

} // namespace model

#endif /* end of include guard: MODEL_HPP */
