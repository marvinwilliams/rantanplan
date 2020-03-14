#ifndef NORMALIZE_MODEL_HPP
#define NORMALIZE_MODEL_HPP

#include "util/index.hpp"
#include "util/tagged_union.hpp"

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

struct Constant;
using ConstantIndex = util::Index<Constant>;

struct Constant {
  TypeIndex type;
};

struct Predicate;
using PredicateIndex = util::Index<Predicate>;

struct Predicate {
  std::vector<TypeIndex> parameter_types;
};

struct Parameter : TaggedUnion<ConstantIndex, TypeIndex> {
  using TaggedUnion<ConstantIndex, TypeIndex>::TaggedUnion;
  inline ConstantIndex get_constant() const noexcept { return get_first(); }
  inline TypeIndex get_type() const noexcept { return get_second(); }
  inline bool is_free() const noexcept { return !holds_t1_; }
};

using ParameterIndex = util::Index<Parameter>;

struct Argument : TaggedUnion<ConstantIndex, ParameterIndex> {
  using TaggedUnion<ConstantIndex, ParameterIndex>::TaggedUnion;
  inline ConstantIndex get_constant() const noexcept { return get_first(); }
  inline ParameterIndex get_parameter_index() const noexcept {
    return get_second();
  }
  inline bool is_parameter() const noexcept { return !holds_t1_; }
};

using ArgumentIndex = util::Index<Argument>;

struct Atom {
  PredicateIndex predicate;
  std::vector<Argument> arguments;
};

struct GroundAtom {
  PredicateIndex predicate;
  std::vector<ConstantIndex> arguments;
};

inline bool operator==(const GroundAtom &first,
                       const GroundAtom &second) noexcept {
  return first.predicate == second.predicate &&
         first.arguments == second.arguments;
}

struct Condition {
  Atom atom;
  bool positive = true;
};

struct Action;
using ActionIndex = util::Index<Action>;

struct Action {
  ActionIndex id;
  std::vector<Parameter> parameters;
  std::vector<Condition> preconditions;
  std::vector<std::pair<GroundAtom, bool>> ground_preconditions;
  std::vector<Condition> effects;
  std::vector<std::pair<GroundAtom, bool>> ground_effects;
};

struct Problem {
  std::string domain_name;
  std::string problem_name;
  std::vector<std::string> requirements;
  std::vector<Type> types;
  std::vector<std::string> type_names;
  std::vector<Constant> constants;
  std::vector<std::string> constant_names;
  std::vector<std::vector<ConstantIndex>> constants_of_type;
  std::vector<std::unordered_map<ConstantIndex, size_t>> constant_type_map;
  std::vector<Predicate> predicates;
  std::vector<std::string> predicate_names;
  std::vector<Action> actions;
  std::vector<std::string> action_names;
  std::vector<GroundAtom> init;
  std::vector<std::pair<GroundAtom, bool>> goal;

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

template <> struct hash<normalized::GroundAtom> {
  size_t operator()(const normalized::GroundAtom &atom) const noexcept {
    size_t h = hash<normalized::PredicateIndex>{}(atom.predicate);
    for (auto c : atom.arguments) {
      h ^= hash<normalized::ConstantIndex>{}(c);
    }
    return h;
  }
};

} // namespace std

#endif /* end of include guard: NORMALIZE_MODEL_HPP */
