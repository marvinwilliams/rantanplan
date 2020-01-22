#ifndef PARALLEL_PREPROCESS_HPP
#define PARALLEL_PREPROCESS_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "planner/planner.hpp"
#include "util/index.hpp"
#include "util/timer.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <utility>

extern logging::Logger preprocess_logger;
extern Config config;
extern const util::Timer global_timer;
extern std::atomic_bool thread_stop_flag;

class ParallelPreprocessor {
public:
  struct predicate_id_t {};
  using PredicateId = util::Index<predicate_id_t>;
  enum class Status { Success, Timeout, Interrupt };

  explicit ParallelPreprocessor(
      unsigned int num_threads,
      const std::shared_ptr<normalized::Problem> &problem) noexcept;

  Status get_status() const noexcept;
  void refine(float progress, std::chrono::seconds timeout,
              unsigned int num_threads) noexcept;
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

  void prune_actions(unsigned int num_threads) noexcept;
  bool is_valid(const normalized::Action &action) const noexcept;
  bool simplify(normalized::Action &action) const noexcept;

  Status status_ = Status::Success;
  float progress_;
  uint_fast64_t num_actions_;
  std::atomic_uint_fast64_t num_pruned_actions_ = 0;
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
    std::mutex pos_rigid_mutex;
    std::mutex neg_rigid_mutex;
    std::mutex pos_effectless_mutex;
    std::mutex neg_effectless_mutex;
  };

  mutable std::vector<Cache> successful_cache_;
  mutable std::vector<Cache> unsuccessful_cache_;

  decltype(&ParallelPreprocessor::select_free) parameter_selector_;
  std::shared_ptr<normalized::Problem> problem_;
};

#endif /* end of include guard: PARALLEL_PREPROCESS_HPP */
