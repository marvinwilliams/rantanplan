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

class Support {
public:
  struct ConditionSupport {
    std::vector<
        std::pair<const normalized::Action *, normalized::ParameterAssignment>>
        pos_precondition;
    std::vector<
        std::pair<const normalized::Action *, normalized::ParameterAssignment>>
        neg_precondition;
    std::vector<
        std::pair<const normalized::Action *, normalized::ParameterAssignment>>
        pos_effect;
    std::vector<
        std::pair<const normalized::Action *, normalized::ParameterAssignment>>
        neg_effect;
  };
  static logging::Logger logger;

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

  inline size_t get_num_instantiations() const noexcept {
    return num_instantations_;
  }

  inline normalized::InstantiationHandle get_predicate_handle(
      const normalized::PredicateInstantiation &predicate) const noexcept {
    auto [it, success] =
        instantiations_.try_emplace(predicate, instantiations_.size());
    return it->second;
  }

  inline const auto &get_condition_supports() const noexcept {
    return condition_supports_;
  }

  inline const auto &get_support(normalized::InstantiationHandle handle,
                                 bool positive, bool is_effect) const noexcept {
    return positive ? is_effect ? condition_supports_[handle].pos_effect
                                : condition_supports_[handle].pos_precondition
                    : is_effect ? condition_supports_[handle].neg_effect
                                : condition_supports_[handle].neg_precondition;
  }

  inline bool is_init(normalized::InstantiationHandle handle) const noexcept {
    return init_.find(handle) != init_.end();
  }

  bool is_rigid(normalized::InstantiationHandle handle, bool positive) const
      noexcept {
    return (positive ? condition_supports_[handle].neg_effect.empty()
                     : condition_supports_[handle].pos_effect.empty()) &&
           is_init(handle) == positive;
  }

private:
  inline auto &select_support(normalized::InstantiationHandle handle,
                              bool positive, bool is_effect) noexcept {
    return positive ? is_effect ? condition_supports_[handle].pos_effect
                                : condition_supports_[handle].pos_precondition
                    : is_effect ? condition_supports_[handle].neg_effect
                                : condition_supports_[handle].neg_precondition;
  }

  void set_predicate_support() noexcept;

  std::unordered_set<normalized::InstantiationHandle> init_;
  size_t num_instantations_;
  mutable std::unordered_map<normalized::PredicateInstantiation,
                             normalized::InstantiationHandle>
      instantiations_;
  std::vector<ConditionSupport> condition_supports_;

  const normalized::Problem &problem_;
};

#endif /* end of include guard: SUPPORT_HPP */
