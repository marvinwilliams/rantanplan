#ifndef PREPROCESS_HPP
#define PREPROCESS_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "planner/planner.hpp"
#include "util/index.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <utility>

extern logging::Logger preprocess_logger;

class Preprocessor {
public:
  struct predicate_id_t {};
  using PredicateId = util::Index<predicate_id_t>;

  explicit Preprocessor(const std::shared_ptr<normalized::Problem> &problem,
                        const Config &config) noexcept;

  bool refine() noexcept;
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

  enum class SimplifyResult { Unchanged, Changed, Invalid };
  SimplifyResult simplify(normalized::Action &action) const noexcept;

  float preprocess_progress_;
  uint_fast64_t num_actions_;
  uint_fast64_t num_pruned_actions_ = 0;
  std::vector<std::vector<normalized::Action>> partially_instantiated_actions_;
  std::vector<uint_fast64_t> predicate_id_offset_;
  std::vector<bool> trivially_rigid_;
  std::vector<bool> trivially_effectless_;
  std::vector<PredicateId> init_;
  std::vector<std::pair<PredicateId, bool>> goal_;

  struct Cache {
    std::unordered_set<PredicateId> pos_rigid;
    std::unordered_set<PredicateId> neg_rigid;
    std::unordered_set<PredicateId> pos_effectless;
    std::unordered_set<PredicateId> neg_effectless;
  };

  const Config &config_;
  decltype(&Preprocessor::select_free) parameter_selector_;
  std::shared_ptr<normalized::Problem> problem_;
};

#endif /* end of include guard: PREPROCESS_HPP */
