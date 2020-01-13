#ifndef NORMALIZE_MODEL_HPP
#define NORMALIZE_MODEL_HPP

#include "util/index.hpp"

#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace normalized {

struct Type;
using TypeIndex = util::Index<Type>;

struct Type {
  TypeIndex supertype;
};

struct Constant {
  TypeIndex type;
};

using ConstantIndex = util::Index<Constant>;

struct Predicate {
  std::vector<TypeIndex> parameter_types;
};

using PredicateIndex = util::Index<Predicate>;

class Parameter {
public:
  explicit Parameter(ConstantIndex c) : is_constant_{true}, constant_idx_{c} {}
  explicit Parameter(TypeIndex t) : is_constant_{false}, type_idx_{t} {}
  explicit Parameter(const Parameter &other)
      : is_constant_{other.is_constant_} {
    if (other.is_constant_) {
      constant_idx_ = other.constant_idx_;
    } else {
      type_idx_ = other.type_idx_;
    }
  }

  Parameter &operator=(const Parameter &other) {
    is_constant_ = other.is_constant_;
    if (other.is_constant_) {
      constant_idx_ = other.constant_idx_;
    } else {
      type_idx_ = other.type_idx_;
    }
    return *this;
  }

  void set(ConstantIndex c) {
    is_constant_ = true;
    constant_idx_ = c;
  }

  void set(TypeIndex t) {
    is_constant_ = false;
    type_idx_ = t;
  }

  ConstantIndex get_constant() const {
    assert(is_constant_);
    return constant_idx_;
  }

  TypeIndex get_type() const {
    assert(!is_constant_);
    return type_idx_;
  }

  bool is_constant() const { return is_constant_; }

private:
  bool is_constant_;
  union {
    ConstantIndex constant_idx_;
    TypeIndex type_idx_;
  };
};

using ParameterIndex = util::Index<Parameter>;

class Argument {
public:
  explicit Argument(ConstantIndex c) : is_constant_{true}, constant_idx_{c} {}
  explicit Argument(ParameterIndex p)
      : is_constant_{false}, parameter_idx_{p} {}
  explicit Argument(const Argument &other) : is_constant_{other.is_constant_} {
    if (other.is_constant_) {
      constant_idx_ = other.constant_idx_;
    } else {
      parameter_idx_ = other.parameter_idx_;
    }
  }

  Argument &operator=(const Argument &other) {
    is_constant_ = other.is_constant_;
    if (other.is_constant()) {
      constant_idx_ = other.constant_idx_;
    } else {
      parameter_idx_ = other.parameter_idx_;
    }
    return *this;
  }

  void set(ConstantIndex c) {
    is_constant_ = true;
    constant_idx_ = c;
  }

  void set(ParameterIndex p) {
    is_constant_ = false;
    parameter_idx_ = p;
  }

  ConstantIndex get_constant() const {
    assert(is_constant_);
    return constant_idx_;
  }

  ParameterIndex get_parameter() const {
    assert(!is_constant_);
    return parameter_idx_;
  }

  bool is_constant() const { return is_constant_; }

private:
  bool is_constant_;
  union {
    ConstantIndex constant_idx_;
    ParameterIndex parameter_idx_;
  };
};

using ArgumentIndex = util::Index<Argument>;

struct Condition {
  PredicateIndex definition;
  std::vector<Argument> arguments;
  bool positive = true;
};

struct PredicateInstantiation {
  explicit PredicateInstantiation(PredicateIndex definition,
                                  std::vector<ConstantIndex> arguments)
      : definition{definition}, arguments(std::move(arguments)) {}

  PredicateIndex definition;
  std::vector<ConstantIndex> arguments;
};

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

  const Parameter &get(ParameterIndex parameter) const noexcept {
    return parameters[parameter];
  }
};

using ActionIndex = util::Index<Action>;

struct Problem {
  std::string domain_name;
  std::string problem_name;
  std::vector<std::string> requirements;
  std::vector<Type> types;
  std::vector<std::string> type_names;
  std::vector<Constant> constants;
  std::vector<std::string> constant_names;
  std::vector<std::vector<ConstantIndex>> constants_by_type;
  std::vector<Predicate> predicates;
  std::vector<std::string> predicate_names;
  std::vector<Action> actions;
  std::vector<std::string> action_names;
  std::vector<PredicateInstantiation> init;
  std::vector<std::pair<PredicateInstantiation, bool>> goal;

  const Type &get(TypeIndex type) const noexcept { return types[type]; }
  const Constant &get(ConstantIndex constant) const noexcept {
    return constants[constant];
  }
  const Predicate &get(PredicateIndex predicate) const noexcept {
    return predicates[predicate];
  }
  const Action &get(ActionIndex action) const noexcept {
    return actions[action];
  }

  TypeIndex get_index(const Type *type) const noexcept {
    return {static_cast<size_t>(std::distance(&types.front(), type))};
  }
  ConstantIndex get_index(const Constant *constant) const noexcept {
    return {static_cast<size_t>(std::distance(&constants.front(), constant))};
  }
  PredicateIndex get_index(const Predicate *predicate) const noexcept {
    return {static_cast<size_t>(std::distance(&predicates.front(), predicate))};
  }
  ActionIndex get_index(const Action *action) const noexcept {
    return {static_cast<size_t>(std::distance(&actions.front(), action))};
  }

  const std::string &get_name(TypeIndex type) const noexcept {
    return type_names[type];
  }
  const std::string &get_name(ConstantIndex constant) const noexcept {
    return constant_names[constant];
  }
  const std::string &get_name(PredicateIndex predicate) const noexcept {
    return predicate_names[predicate];
  }
  const std::string &get_name(ActionIndex action) const noexcept {
    return action_names[action];
  }
  template <typename T> const std::string &get_name(const T *t) const noexcept {
    return get_name(get_index(t));
  }
};

} // namespace normalized

struct Plan {
  std::vector<std::pair<normalized::ActionIndex,
                        std::vector<normalized::ConstantIndex>>>
      sequence;
  std::shared_ptr<normalized::Problem> problem;
};

namespace std {

template <> struct hash<normalized::PredicateInstantiation> {
  size_t operator()(const normalized::PredicateInstantiation &predicate) const
      noexcept {
    size_t h = hash<normalized::PredicateIndex>{}(predicate.definition);
    for (auto c : predicate.arguments) {
      h ^= hash<normalized::ConstantIndex>{}(c);
    }
    return h;
  }
};

} // namespace std

#endif /* end of include guard: NORMALIZE_MODEL_HPP */
