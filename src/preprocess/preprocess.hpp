#ifndef PREPROCESS_HPP
#define PREPROCESS_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "planner/planner.hpp"
#include "util/index.hpp"
#include "util/timer.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <unordered_set>
#include <utility>

extern logging::Logger preprocess_logger;
extern Config config;

class Preprocessor {
public:
  struct predicate_id_t {};
  using PredicateId = util::Index<predicate_id_t>;

  explicit Preprocessor(
      const std::shared_ptr<normalized::Problem> &problem) noexcept;

  // returns true if progress is reached, false on timeout
  bool refine(float progress, std::chrono::seconds timeout) noexcept;
  size_t get_num_actions() const noexcept;
  float get_progress() const noexcept;
  std::shared_ptr<normalized::Problem> extract_problem() const noexcept;

private:
  PredicateId get_id(const normalized::PredicateInstantiation &predicate) const
      noexcept;
  bool is_trivially_rigid(const normalized::PredicateInstantiation &predicate,
                          bool positive) const noexcept;
  bool
  is_trivially_effectless(const normalized::PredicateInstantiation &predicate,
                          bool positive) const noexcept;
  bool has_effect(const normalized::Action &action,
                  const normalized::PredicateInstantiation &predicate,
                  bool positive) const noexcept;
  bool has_precondition(const normalized::Action &action,
                        const normalized::PredicateInstantiation &predicate,
                        bool positive) const noexcept;
  // No action has this predicate as effect and it is not in init
  bool is_rigid(const normalized::PredicateInstantiation &predicate,
                bool positive) const noexcept;
  // No action has this predicate as precondition and it is a not a goal
  bool is_effectless(const normalized::PredicateInstantiation &predicate,
                     bool positive) const noexcept;
  normalized::ParameterSelection
  select_free(const normalized::Action &action) const noexcept;
  normalized::ParameterSelection
  select_min_new(const normalized::Action &action) const noexcept;
  normalized::ParameterSelection
  select_max_rigid(const normalized::Action &action) const noexcept;

  void prune_actions() noexcept;
  bool is_valid(const normalized::Action &action) const noexcept;
  bool simplify(normalized::Action &action) const noexcept;

  float progress_;
  uint_fast64_t num_actions_;
  uint_fast64_t num_pruned_actions_ = 0;
  std::vector<std::vector<normalized::Action>> actions_;
  std::vector<bool> trivially_rigid_;
  std::vector<bool> trivially_effectless_;
  std::vector<std::vector<PredicateId>> init_;
  std::vector<std::vector<std::pair<PredicateId, bool>>> goal_;

  struct Cache {
    std::unordered_set<PredicateId> pos_rigid;
    std::unordered_set<PredicateId> neg_rigid;
    std::unordered_set<PredicateId> pos_effectless;
    std::unordered_set<PredicateId> neg_effectless;
  };

  mutable std::vector<Cache> successful_cache_;
  mutable std::vector<Cache> unsuccessful_cache_;

  decltype(&Preprocessor::select_free) parameter_selector_;
  std::shared_ptr<normalized::Problem> problem_;
};

#endif /* end of include guard: PREPROCESS_HPP */
