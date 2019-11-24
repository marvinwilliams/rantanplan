#ifndef NORMALIZED_PROBLEM_HPP
#define NORMALIZED_PROBLEM_HPP

#include <cassert>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace normalized {

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

struct Type;
using TypeHandle = Handle<Type>;

struct Type {
  TypeHandle parent;
};

struct Predicate {
  std::vector<TypeHandle> parameter_types;
};

using PredicateHandle = Handle<Predicate>;

struct Constant {
  TypeHandle type;
};

using ConstantHandle = Handle<Constant>;

struct Parameter {
  bool constant;
  size_t index;
};

using ParameterHandle = Handle<Parameter>;

struct Condition {
  PredicateHandle definition;
  std::vector<Parameter> arguments;
  bool positive = true;
};

struct PredicateInstantiation {
  explicit PredicateInstantiation(PredicateHandle definition,
                                  std::vector<ConstantHandle> arguments)
      : definition{definition}, arguments(std::move(arguments)) {}

  PredicateHandle definition;
  std::vector<ConstantHandle> arguments;
};

using PredicateInstantiationHandle = Handle<PredicateInstantiation>;

inline bool operator==(const PredicateInstantiation &first,
                       const PredicateInstantiation &second) {
  return first.definition == second.definition &&
         first.arguments == second.arguments;
}

struct Action {
  std::vector<Parameter> parameters;
  std::vector<Condition> preconditions;
  std::vector<Condition> effects;
  std::vector<std::pair<PredicateInstantiation, bool>> pre_instantiated;
  std::vector<std::pair<PredicateInstantiation, bool>> eff_instantiated;
};

using ActionHandle = Handle<Action>;

struct Problem {
  std::string domain_name;
  std::string problem_name;
  std::vector<std::string> requirements;
  std::vector<Type> types;
  std::vector<std::string> type_names;
  std::vector<Constant> constants;
  std::vector<std::string> constant_names;
  std::vector<Predicate> predicates;
  std::vector<std::string> predicate_names;
  std::vector<Action> actions;
  std::vector<std::string> action_names;
  std::vector<PredicateInstantiation> init;
  std::vector<std::pair<PredicateInstantiation, bool>> goal;
};

} // namespace normalized

namespace std {

template <typename T> struct hash<::normalized::Handle<T>> {
  size_t operator()(const ::normalized::Handle<T> &handle) const noexcept {
    return std::hash<size_t>{}(handle.i);
  }
};

template <> struct hash<normalized::PredicateInstantiation> {
  size_t operator()(const ::normalized::PredicateInstantiation &predicate) const
      noexcept {
    size_t hash =
        std::hash<::normalized::PredicateHandle>{}(predicate.definition);
    for (auto p : predicate.arguments) {
      hash ^= std::hash<::normalized::ConstantHandle>{}(p);
    }
    return hash;
  }
};

} // namespace std

#endif /* end of include guard: NORMALIZED_PROBLEM_HPP */
