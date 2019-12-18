#ifndef PREPROCESS_HPP
#define PREPROCESS_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "util/index.hpp"

#include <unordered_map>

extern logging::Logger preprocess_logger;

class Preprocessor {
public:
  struct predicate_id_t {};
  enum class SimplifyResult { Unchanged, Changed, Invalid };

  using PredicateId = Index<predicate_id_t>;

  explicit Preprocessor(const normalized::Problem &problem) noexcept;

  normalized::Problem preprocess(const Config &config) noexcept;

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

  template <typename Function> void refine(Function &&select_parameters) {
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
        if (auto result = simplify(action);
            result != SimplifyResult::Unchanged) {
          refinement_possible_ = true;
          if (result == SimplifyResult::Invalid) {
            num_pruned_actions_ +=
                normalized::get_num_instantiated(action, problem_);
            continue;
          }
        }

        auto selection = select_parameters(action);

        if (selection.empty()) {
          new_actions.push_back(action);
          num_current_actions += 1;
          continue;
        }

        refinement_possible_ = true;

        for_each_instantiation(
            selection, action,
            [&](const normalized::ParameterAssignment &assignment) {
              auto new_action = ground(assignment, action);
              if (auto result = simplify(new_action);
                  result != SimplifyResult::Invalid) {
                new_actions.push_back(std::move(new_action));
                num_current_actions += 1;
              } else {
                num_pruned_actions_ +=
                    normalized::get_num_instantiated(new_action, problem_);
              }
            },
            problem_);
      }
      partially_instantiated_actions_[i] = std::move(new_actions);
    }
    LOG_INFO(preprocess_logger, "Progress: %f",
             static_cast<double>(num_current_actions + num_pruned_actions_) /
                 static_cast<double>(num_actions_));
  }

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
  mutable std::unordered_map<PredicateId, bool> rigid_tested_;
  mutable std::unordered_map<PredicateId, bool> effectless_;
  mutable std::unordered_map<PredicateId, bool> effectless_tested_;
  const normalized::Problem &problem_;
};

#endif /* end of include guard: PREPROCESS_HPP */
