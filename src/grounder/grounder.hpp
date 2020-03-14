#ifndef GROUNDER_HPP
#define GROUNDER_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "planner/planner.hpp"
#include "util/index.hpp"
#include "util/timer.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <utility>

extern logging::Logger grounder_logger;
extern Config config;
extern util::Timer global_timer;

class Grounder {
public:
  struct predicate_id_t {};
  using PredicateId = util::Index<predicate_id_t>;

  explicit Grounder(
      const std::shared_ptr<normalized::Problem> &problem) noexcept;

  void refine(float groundness, util::Seconds timeout);
  size_t get_num_actions() const noexcept;
  float get_groundness() const noexcept;
  std::shared_ptr<normalized::Problem> extract_problem() const noexcept;

private:
  PredicateId get_id(const normalized::GroundAtom &predicate) const noexcept;
  bool is_trivially_rigid(const normalized::GroundAtom &predicate,
                          bool positive) const noexcept;
  bool is_trivially_useless(const normalized::GroundAtom &predicate) const
      noexcept;
  bool has_precondition(const normalized::Action &action,
                        const normalized::GroundAtom &predicate) const noexcept;
  bool has_effect(const normalized::Action &action,
                  const normalized::GroundAtom &predicate, bool positive) const
      noexcept;

  // No action has this predicate as effect and it is not in init_
  template <bool cache_success, bool cache_fail>
  bool is_rigid(const normalized::GroundAtom &atom, bool positive) const
      noexcept {
    auto &rigid = (positive ? successful_cache_[atom.predicate].pos_rigid
                            : successful_cache_[atom.predicate].neg_rigid);
    auto &not_rigid =
        (positive ? unsuccessful_cache_[atom.predicate].pos_rigid
                  : unsuccessful_cache_[atom.predicate].neg_rigid);
    auto id = get_id(atom);

    if (cache_success && (rigid.find(id) != rigid.end())) {
      return true;
    }

    if (cache_fail && (not_rigid.find(id) != not_rigid.end())) {
      return false;
    }

    if (std::binary_search(init_[atom.predicate].begin(),
                           init_[atom.predicate].end(), id) != positive) {
      if (cache_fail) {
        not_rigid.insert(id);
      }
      return false;
    }

    if (trivially_rigid_[atom.predicate]) {
      if (cache_success) {
        rigid.insert(id);
      }
      return true;
    }

    if (config.pruning_policy == Config::PruningPolicy::Trivial) {
      if (cache_fail) {
        not_rigid.insert(id);
      }
      return false;
    }

    for (size_t i = 0; i < problem_->actions.size(); ++i) {
      const auto &base_action = problem_->actions[i];
      if (!has_effect(base_action, atom, !positive)) {
        continue;
      }
      for (const auto &action : actions_[i]) {
        if (has_effect(action, atom, !positive)) {
          if (cache_fail) {
            not_rigid.insert(id);
          }
          return false;
        }
      }
    }
    if (cache_success) {
      rigid.insert(id);
    }
    return true;
  }

  bool is_rigid(const normalized::GroundAtom &atom, bool positive) const
      noexcept;

  // No action has this predicate as precondition and it is a not a goal
  template <bool cache_success, bool cache_fail>
  bool is_useless(const normalized::GroundAtom &atom) const noexcept {
    auto id = get_id(atom);
    auto &useless = successful_cache_[atom.predicate].useless;
    auto &not_useless = unsuccessful_cache_[atom.predicate].useless;

    if (cache_success && (useless.find(id) != useless.end())) {
      return true;
    }

    if (cache_fail && (not_useless.find(id) != not_useless.end())) {
      return false;
    }

    if (std::binary_search(goal_[atom.predicate].begin(),
                           goal_[atom.predicate].end(), id)) {
      if (cache_fail) {
        not_useless.insert(id);
      }
      return false;
    }

    if (trivially_useless_[atom.predicate]) {
      if (cache_success) {
        useless.insert(id);
      }
      return true;
    }

    if (config.pruning_policy == Config::PruningPolicy::Trivial) {
      if (cache_fail) {
        not_useless.insert(id);
      }
      return false;
    }

    for (size_t i = 0; i < problem_->actions.size(); ++i) {
      const auto &action = problem_->actions[i];
      if (has_precondition(action, atom)) {
        for (const auto &action : actions_[i]) {
          if (has_precondition(action, atom)) {
            if (cache_fail) {
              not_useless.insert(id);
            }
            return false;
          }
        }
      }
    }

    if (cache_success) {
      useless.insert(id);
    }
    return true;
  }

  bool is_useless(const normalized::GroundAtom &atom) const noexcept;

  normalized::ParameterSelection
  select_most_frequent(const normalized::Action &action) const noexcept;
  normalized::ParameterSelection
  select_min_new(const normalized::Action &action) const noexcept;
  normalized::ParameterSelection
  select_max_rigid(const normalized::Action &action) const noexcept;
  normalized::ParameterSelection
  select_approx_min_new(const normalized::Action &action) const noexcept;
  normalized::ParameterSelection
  select_approx_max_rigid(const normalized::Action &action) const noexcept;
  normalized::ParameterSelection
  select_first_effect(const normalized::Action &action) const noexcept;

  void prune_actions() noexcept;
  bool is_valid(const normalized::Action &action) const noexcept;
  std::pair<normalized::Action, bool> ground(const normalized::Action &action,
              const normalized::ParameterAssignment &assignment) const noexcept;
  bool simplify(normalized::Action &action) const noexcept;

  float groundness_;
  uint_fast64_t num_actions_;
  uint_fast64_t num_pruned_actions_ = 0;
  std::vector<std::vector<normalized::Action>> actions_;
  std::vector<bool> trivially_rigid_;
  std::vector<bool> trivially_useless_;
  std::vector<std::vector<PredicateId>> init_;
  std::vector<std::vector<PredicateId>> goal_;
  std::vector<bool> action_grounded_;

  struct Cache {
    std::unordered_set<PredicateId> pos_rigid;
    std::unordered_set<PredicateId> neg_rigid;
    std::unordered_set<PredicateId> useless;
  };

  mutable std::vector<Cache> successful_cache_;
  mutable std::vector<Cache> unsuccessful_cache_;

  decltype(&Grounder::select_most_frequent) parameter_selector_;
  std::shared_ptr<normalized::Problem> problem_;
};

#endif /* end of include guard: GROUNDER_HPP */
