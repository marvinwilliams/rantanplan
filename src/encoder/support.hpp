#ifndef SUPPORT_HPP
#define SUPPORT_HPP

#include "config.hpp"
#include "grounder/grounder.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "model/to_string.hpp"
#include "util/index.hpp"
#include "util/timer.hpp"

#include <numeric>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

extern logging::Logger encoding_logger;
extern Config config;
extern util::Timer global_timer;

class Support {
public:
  struct predicate_id_t {};
  using PredicateId = util::Index<predicate_id_t>;
  struct ConditionSupport {
    std::vector<
        std::pair<normalized::ActionIndex, normalized::ParameterAssignment>>
        pos_precondition;
    std::vector<
        std::pair<normalized::ActionIndex, normalized::ParameterAssignment>>
        neg_precondition;
    std::vector<
        std::pair<normalized::ActionIndex, normalized::ParameterAssignment>>
        pos_effect;
    std::vector<
        std::pair<normalized::ActionIndex, normalized::ParameterAssignment>>
        neg_effect;
  };

  explicit Support(const normalized::Problem &problem, util::Seconds timeout);

  inline const normalized::Problem &get_problem() const noexcept {
    return problem_;
  }

  inline size_t get_num_ground_atoms() const noexcept {
    return num_ground_atoms_;
  }

  inline PredicateId get_id(const normalized::GroundAtom &atom) const noexcept {
    auto [it, success] = ground_atoms_.try_emplace(atom, ground_atoms_.size());
    assert(it->second < num_ground_atoms_);
    return it->second;
  }

  inline const auto &get_condition_supports() const noexcept {
    return condition_supports_;
  }

  inline const auto &get_support(PredicateId id, bool positive,
                                 bool is_effect) const noexcept {
    return positive ? is_effect ? condition_supports_[id].pos_effect
                                : condition_supports_[id].pos_precondition
                    : is_effect ? condition_supports_[id].neg_effect
                                : condition_supports_[id].neg_precondition;
  }

  inline bool is_init(PredicateId id) const noexcept {
    return init_.find(id) != init_.end();
  }

  bool is_rigid(PredicateId id, bool positive) const noexcept {
    return (positive ? condition_supports_[id].neg_effect.empty()
                     : condition_supports_[id].pos_effect.empty()) &&
           is_init(id) == positive;
  }

private:
  inline auto &select_support(PredicateId id, bool positive,
                              bool is_effect) noexcept {
    return positive ? is_effect ? condition_supports_[id].pos_effect
                                : condition_supports_[id].pos_precondition
                    : is_effect ? condition_supports_[id].neg_effect
                                : condition_supports_[id].neg_precondition;
  }

  void set_predicate_support();

  util::Timer timer_;
  util::Seconds timeout_;
  size_t num_ground_atoms_;
  std::unordered_set<PredicateId> init_;
  mutable std::unordered_map<normalized::GroundAtom, PredicateId> ground_atoms_;
  std::vector<ConditionSupport> condition_supports_;

  const normalized::Problem &problem_;
};

#endif /* end of include guard: SUPPORT_HPP */
