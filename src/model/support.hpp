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

namespace support {

extern logging::Logger logger;

class Support {
public:
  using PredicateSupport =
      std::vector<std::unordered_map<model::ActionHandle,
                                     std::vector<model::ParameterAssignment>,
                                     model::hash::Handle<model::Action>>>;

  explicit Support(const model::Problem &problem_) noexcept;
  Support(const Support &support) = delete;
  Support &operator=(const Support &support) = delete;
  Support(Support &&support) = default;
  Support &operator=(Support &&support) = delete;

  void update() noexcept;

  inline const model::Problem &get_problem() const noexcept { return problem_; }

  inline size_t get_num_predicates() const noexcept {
    return problem_.predicates.size();
  }

  inline size_t get_num_actions() const noexcept {
    return problem_.actions.size();
  }

  inline const auto &get_ground_predicates() const noexcept {
    assert(ground_predicates_constructed_);
    return ground_predicates_;
  }

  inline model::GroundPredicateHandle
  get_predicate_index(const model::GroundPredicate &predicate) const noexcept {
    assert(ground_predicates_constructed_ &&
           ground_predicates_.at(predicate.definition).count(predicate) > 0);
    return ground_predicates_.at(predicate);
  }

  inline const PredicateSupport &get_predicate_support(bool is_negated,
                                                       bool is_effect) const
      noexcept {
    assert(predicate_support_constructed_);
    if (is_negated) {
      return is_effect ? neg_effect_support_ : neg_precondition_support_;
    } else {
      return is_effect ? pos_effect_support_ : pos_precondition_support_;
    }
  }

  inline bool is_init(model::GroundPredicateHandle predicate_ptr) const
      noexcept {
    return initial_state_.count(predicate_ptr) > 0;
  }

  bool is_rigid(model::GroundPredicateHandle predicate_ptr, bool negated) const
      noexcept {
    assert(predicate_support_constructed_);
    return (negated ? neg_rigid_predicates_.count(predicate_ptr)
                    : pos_rigid_predicates_.count(predicate_ptr)) == 1;
  }

  inline auto get_rigid(bool negated) {
    return negated ? neg_rigid_predicates_ : pos_rigid_predicates_;
  }

  bool simplify_action(model::Action &action) noexcept;

private:
  inline PredicateSupport &select_support(bool is_negated,
                                          bool is_effect) noexcept {
    if (is_negated) {
      return is_effect ? neg_effect_support_ : neg_precondition_support_;
    } else {
      return is_effect ? pos_effect_support_ : pos_precondition_support_;
    }
  }

  void ground_predicates() noexcept;

  void set_predicate_support() noexcept;

  std::unordered_set<model::GroundPredicateHandle,
                     model::hash::Handle<model::GroundPredicate>>
      initial_state_;
  std::unordered_map<model::GroundPredicate, model::GroundPredicateHandle,
                     model::hash::GroundPredicate>
      ground_predicates_;
  PredicateSupport pos_precondition_support_;
  PredicateSupport neg_precondition_support_;
  PredicateSupport pos_effect_support_;
  PredicateSupport neg_effect_support_;
  std::unordered_set<model::GroundPredicateHandle,
                     model::hash::Handle<model::GroundPredicate>>
      pos_rigid_predicates_;
  std::unordered_set<model::GroundPredicateHandle,
                     model::hash::Handle<model::GroundPredicate>>
      neg_rigid_predicates_;
  bool ground_predicates_constructed_ = false;
  bool predicate_support_constructed_ = false;

  const model::Problem &problem_;
};

} // namespace support

#endif /* end of include guard: SUPPORT_HPP */
