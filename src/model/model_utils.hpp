#ifndef MODEL_UTILS_HPP
#define MODEL_UTILS_HPP

#include "model/model.hpp"

#include "util/combinatorics.hpp"
#include <algorithm>
#include <cassert>
#include <variant>
#include <vector>

namespace model {

inline bool is_subtype(const std::vector<Type> &types, TypePtr type,
                       TypePtr supertype) {
  if (type == supertype) {
    return true;
  }
  while (types[type].parent != type) {
    type = types[type].parent;
    if (type == supertype) {
      return true;
    }
  }
  return false;
}

inline bool is_grounded(const PredicateEvaluation &predicate) noexcept {
  return std::all_of(
      predicate.arguments.cbegin(), predicate.arguments.cend(),
      [](const auto &a) { return std::holds_alternative<ConstantPtr>(a); });
}

inline bool is_grounded(const PredicateEvaluation &predicate,
                        const Action &action) noexcept {
  return std::all_of(
      predicate.arguments.cbegin(), predicate.arguments.cend(),
      [&action](const auto &a) {
        return std::holds_alternative<ConstantPtr>(a) ||
               action.parameters[std::get<ParameterPtr>(a)].constant;
      });
}

inline bool is_grounded(const Action &action) {
  return std::all_of(action.parameters.cbegin(), action.parameters.cend(),
                     [](const auto &p) { return p.constant.has_value(); });
}

inline bool is_unifiable(const GroundPredicate &grounded_predicate,
                         const PredicateEvaluation &predicate) {
  if (grounded_predicate.definition != predicate.definition) {
    return false;
  }
  return std::mismatch(
             grounded_predicate.arguments.cbegin(),
             grounded_predicate.arguments.cend(), predicate.arguments.cbegin(),
             [](const auto &constant, const auto &argument) {
               const auto const_a = std::get_if<ConstantPtr>(&argument);
               return const_a == nullptr || constant == *const_a;
             })
             .first == grounded_predicate.arguments.cend();
}

inline bool holds(const State &state, const GroundPredicate &predicate,
                  bool negated = false) {
  bool in_state =
      std::any_of(state.predicates.cbegin(), state.predicates.cend(),
                  [&predicate](const auto &state_predicate) {
                    return state_predicate == predicate;
                  });
  return negated != in_state;
}

inline bool holds(const State &state, const PredicateEvaluation &predicate) {
  bool in_state =
      std::any_of(state.predicates.cbegin(), state.predicates.cend(),
                  [&predicate](const auto &state_predicate) {
                    return is_unifiable(state_predicate, predicate);
                  });
  return predicate.negated != in_state;
}

inline bool holds(const RelaxedState &state, const GroundPredicate &predicate,
                  bool negated) {
  if (state.predicates.find(predicate) == state.predicates.end()) {
    return holds(state.initial_state, predicate, negated);
  }
  return true;
}

} // namespace model

#endif /* end of include guard: MODEL_UTILS_HPP */
