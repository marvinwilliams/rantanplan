#ifndef SUPPORT_HPP
#define SUPPORT_HPP

#include "logging/logging.hpp"
#include "model/normalized_problem.hpp"
#include "model/to_string.hpp"
#include "model/utils.hpp"

#include <numeric>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

extern logging::Logger support_logger;

class Support {
public:
  using PredicateSupport = std::vector<std::unordered_map<
      normalized::ActionHandle, std::vector<normalized::ParameterAssignment>>>;

  explicit Support(const normalized::Problem &problem_) noexcept;
  Support(const Support &support) = delete;
  Support &operator=(const Support &support) = delete;
  Support(Support &&support) = default;
  Support &operator=(Support &&support) = delete;

  inline const normalized::Problem &get_problem() const noexcept {
    return problem_;
  }

  inline size_t get_num_predicates() const noexcept {
    return problem_.predicates.size();
  }

  inline size_t get_num_actions() const noexcept {
    return problem_.actions.size();
  }

  inline const std::vector<normalized::ConstantHandle> &
  get_constants_of_type(normalized::TypeHandle type) const {
    return constants_by_type_[type];
  }

  inline const auto &get_instantiations() const noexcept {
    assert(instantiated_);
    return instantiations_;
  }

  inline normalized::InstantiationHandle
  get_predicate_index(const normalized::PredicateInstantiation &predicate) const
      noexcept {
    assert(instantiated_ && instantiations_.count(predicate) > 0);
    return instantiations_.at(predicate);
  }

  inline const PredicateSupport &get_predicate_support(bool positive,
                                                       bool is_effect) const
      noexcept {
    assert(predicate_support_constructed_);
    if (positive) {
      return is_effect ? pos_effect_support_ : pos_precondition_support_;
    } else {
      return is_effect ? neg_effect_support_ : neg_precondition_support_;
    }
  }

  inline bool is_init(normalized::InstantiationHandle handle) const noexcept {
    return init_.find(handle) != init_.end();
  }

  bool is_rigid(normalized::InstantiationHandle handle, bool positive) const
      noexcept {
    assert(predicate_support_constructed_);
    return (positive ? pos_rigid_predicates_.count(handle)
                     : neg_rigid_predicates_.count(handle)) == 1;
  }

  inline auto get_rigid(bool positive) {
    return positive ? pos_rigid_predicates_ : neg_rigid_predicates_;
  }

  bool simplify_action(normalized::Action &action) noexcept;

private:
  inline PredicateSupport &select_support(bool positive,
                                          bool is_effect) noexcept {
    if (positive) {
      return is_effect ? pos_effect_support_ : pos_precondition_support_;
    } else {
      return is_effect ? neg_effect_support_ : neg_precondition_support_;
    }
  }

  void instantiate_predicates() noexcept;

  void set_predicate_support() noexcept;

  bool instantiated_ = false;
  bool predicate_support_constructed_ = false;

  std::vector<std::vector<normalized::ConstantHandle>> constants_by_type_;
  std::unordered_set<normalized::InstantiationHandle> init_;
  std::unordered_map<normalized::PredicateInstantiation,
                     normalized::InstantiationHandle>
      instantiations_;
  PredicateSupport pos_precondition_support_;
  PredicateSupport neg_precondition_support_;
  PredicateSupport pos_effect_support_;
  PredicateSupport neg_effect_support_;
  std::unordered_set<normalized::InstantiationHandle> pos_rigid_predicates_;
  std::unordered_set<normalized::InstantiationHandle> neg_rigid_predicates_;

  const normalized::Problem &problem_;
};

#endif /* end of include guard: SUPPORT_HPP */
