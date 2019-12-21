#ifndef PREPROCESS_HPP
#define PREPROCESS_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "planning/planner.hpp"
#include "util/index.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <execution>
#include <pstl/glue_execution_defs.h>
#include <unordered_map>
#include <utility>
#ifdef PARALLEL
#include <mutex>
#include <pthread.h>
#include <thread>
#endif

extern logging::Logger preprocess_logger;

class Preprocessor {
public:
  struct predicate_id_t {};
  enum class SimplifyResult { Unchanged, Changed, Invalid };

  using PredicateId = Index<predicate_id_t>;

  explicit Preprocessor(const normalized::Problem &problem) noexcept;

#ifdef PARALLEL
  std::pair<normalized::Problem, Planner::Plan>
  preprocess(const Planner &planner, const Config &config) noexcept;

  void kill() {
    std::for_each(threads_.begin(), threads_.end(), [](auto &t) {
      auto id = t.native_handle();
      t.detach();
      pthread_cancel(id);
    });
  }
#else
  normalized::Problem preprocess(const Config &config) noexcept;
#endif

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
  SimplifyResult simplify(normalized::Action &action) noexcept;

  template <typename Function>
  std::vector<normalized::Action>
  refine_action(normalized::ActionIndex action_index,
                Function &&select_parameters, std::atomic_size_t &inst_index,
                std::atomic_flag &changed) {
    std::vector<normalized::Action> new_actions;
    std::unordered_map<PredicateId, bool> not_rigid;
    std::unordered_map<PredicateId, bool> not_effectless;
    uint_fast64_t pruned_actions = 0;
    uint_fast64_t current_actions = 0;
    size_t i;
    while ((i = inst_index++ <
                partially_instantiated_actions_[action_index].size())) {
      auto &action = partially_instantiated_actions_[action_index][i];
      if (auto result = simplify(action); result != SimplifyResult::Unchanged) {
        changed.test_and_set();
        if (result == SimplifyResult::Invalid) {
          pruned_actions += normalized::get_num_instantiated(action, problem_);
          continue;
        }
      }

      auto selection = select_parameters(action);

      if (selection.empty()) {
        new_actions.push_back(action);
        current_actions += 1;
        continue;
      }

      changed.test_and_set();

      for_each_instantiation(
          selection, action,
          [&](const normalized::ParameterAssignment &assignment) {
            auto new_action = ground(assignment, action);
            if (auto result = simplify(new_action);
                result != SimplifyResult::Invalid) {
              new_actions.push_back(std::move(new_action));
              current_actions += 1;
            } else {
              pruned_actions +=
                  normalized::get_num_instantiated(new_action, problem_);
            }
          },
          problem_);
    }
  }

  template <typename Function> float refine(Function &&select_parameters) {
    LOG_INFO(preprocess_logger, "New grounding iteration with %lu action(s)",
             std::accumulate(partially_instantiated_actions_.begin(),
                             partially_instantiated_actions_.end(), 0ul,
                             [](size_t sum, const auto &actions) {
                               return sum + actions.size();
                             }));

    refinement_possible_ = false;
    rigid_tested_.clear();
    effectless_tested_.clear();
    uint_fast64_t num_current_actions = 0;
    for (size_t i = 0; i < problem_.actions.size(); ++i) {
      std::vector<normalized::Action> new_actions;
      for (auto &action : partially_instantiated_actions_[i]) {
      }
      partially_instantiated_actions_[i] = std::move(new_actions);
    }
    LOG_INFO(preprocess_logger, "Progress: %f",
             static_cast<double>(num_current_actions + num_pruned_actions_) /
                 static_cast<double>(num_actions_));
    return static_cast<float>(num_current_actions + num_pruned_actions_) /
           static_cast<float>(num_actions_);
  }

  normalized::Problem to_problem() const noexcept;

  bool refinement_possible_ = true;
  uint_fast64_t num_actions_;
  uint_fast64_t num_pruned_actions_ = 0;
  std::vector<uint_fast64_t> predicate_id_offset_;
  std::vector<bool> trivially_rigid_;
  std::vector<bool> trivially_effectless_;
  std::vector<std::vector<normalized::Action>> partially_instantiated_actions_;
  std::vector<PredicateId> init_;
  std::vector<std::pair<PredicateId, bool>> goal_;
  mutable std::unordered_map<PredicateId, bool> rigid_;
  mutable std::unordered_map<PredicateId, bool> effectless_;
#ifdef PARALLEL
  std::vector<std::thread> threads_;
  unsigned int num_free_threads_;
  mutable std::mutex rigid_mutex_;
  mutable std::mutex effectless_mutex_;
#endif
  const normalized::Problem &problem_;
};

#endif /* end of include guard: PREPROCESS_HPP */
