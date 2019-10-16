#ifndef SUPPORT_HPP
#define SUPPORT_HPP

#include "logging/logging.hpp"
#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "model/to_string.hpp"

#include <numeric>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace model {

namespace support {

extern logging::Logger logger;

struct ArgumentMapping {
  explicit ArgumentMapping(const std::vector<Parameter> &parameters,
                           const std::vector<Argument> &arguments) noexcept;

  inline size_t size() const noexcept { return matches.size(); }

  inline size_t get_parameter_index(size_t pos) const noexcept {
    assert(pos < matches.size());
    return matches[pos].first;
  }

  inline const auto &get_argument_matches(size_t pos) const noexcept {
    assert(pos < matches.size());
    return matches[pos].second;
  }

  std::vector<std::pair<size_t, std::vector<size_t>>> matches;
};

struct ArgumentAssignment {
  explicit ArgumentAssignment(const ArgumentMapping &mapping,
                              const std::vector<size_t> &arguments) noexcept;

  inline const auto &get_arguments() const noexcept { return arguments; }

  std::vector<std::pair<size_t, size_t>> arguments;
};

class Support {
public:
  explicit Support(Problem &problem_) noexcept;
  Support(const Support &support) = delete;
  Support &operator=(const Support &support) = delete;
  Support(Support &&support) = default;
  Support &operator=(Support &&support) = delete;

  void update() noexcept;

  inline const Problem &get_problem() const noexcept { return problem_; }

  inline const std::vector<ConstantPtr> &
  get_constants_of_type(TypePtr type) const noexcept {
    assert(type < constants_of_type_.size());
    return constants_of_type_[type];
  }

  inline const auto &get_ground_predicates() const noexcept {
    assert(ground_predicates_constructed_);
    return ground_predicates_;
  }

  inline GroundPredicatePtr
  get_predicate_index(const GroundPredicate &predicate) noexcept {
    assert(ground_predicates_constructed_ &&
           ground_predicates_.count(predicate) > 0);
    return ground_predicates_[predicate];
  }

  inline const std::vector<
      std::vector<std::pair<ActionPtr, ArgumentAssignment>>> &
  get_predicate_support(bool is_negated, bool is_effect) noexcept {
    assert(predicate_support_constructed_);
    return select_support(is_negated, is_effect);
  }

  inline bool is_rigid(PredicatePtr predicate_ptr) const noexcept {
    assert(predicate_ptr < predicate_in_effect_.size());
    return predicate_in_effect_[predicate_ptr].empty();
  }

  inline bool is_init(const GroundPredicate &predicate) const noexcept {
    return initial_state_.count(predicate) > 0;
  }

  bool is_rigid(const GroundPredicate &predicate, bool negated) noexcept {
    assert(predicate_support_constructed_);
    // Is in initial state
    if (is_init(predicate) == negated) {
      return false;
    }
    if (is_rigid(predicate.definition)) {
      return true;
    }
    // Opposite predicate is in any effect
    return select_support(!negated, true)[get_predicate_index(predicate)]
        .empty();
  }

  inline const std::vector<std::pair<ActionPtr, ArgumentAssignment>> &
  get_forbidden_assignments() noexcept {
    assert(predicate_support_constructed_);
    return forbidden_assignments_;
  }

  inline bool is_grounded(const model::PredicateEvaluation &predicate) const
      noexcept {
    return std::all_of(predicate.arguments.begin(), predicate.arguments.end(),
                       [](const auto &a) {
                         return std::holds_alternative<model::ConstantPtr>(a);
                       });
  }

private:
  inline std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>> &
  select_support(bool is_negated, bool is_effect) noexcept {
    if (is_negated) {
      return is_effect ? neg_effect_support_ : neg_precondition_support_;
    } else {
      return is_effect ? pos_effect_support_ : pos_precondition_support_;
    }
  }

  std::vector<std::vector<ConstantPtr>> sort_constants_by_type() noexcept;

  void ground_predicates() noexcept;

  void ground_action_predicate(const Problem &problem_, ActionPtr action_ptr,
                               const PredicateEvaluation &predicate,
                               bool is_effect) noexcept;

  void set_predicate_support() noexcept;

  std::unordered_set<GroundPredicate, GroundPredicateHash> initial_state_;
  std::vector<std::vector<ConstantPtr>> constants_of_type_;
  std::unordered_map<GroundPredicate, GroundPredicatePtr, GroundPredicateHash>
      ground_predicates_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentMapping>>>
      predicate_in_effect_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>
      pos_precondition_support_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>
      neg_precondition_support_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>
      pos_effect_support_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>
      neg_effect_support_;
  std::vector<std::pair<ActionPtr, ArgumentAssignment>> forbidden_assignments_;
  bool ground_predicates_constructed_ = false;
  bool predicate_support_constructed_ = false;

  const Problem &problem_;
};

} // namespace support

using support::Support;

} // namespace model

#endif /* end of include guard: SUPPORT_HPP */
