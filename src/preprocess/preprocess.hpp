#ifndef PREPROCESS_HPP
#define PREPROCESS_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "util/index.hpp"

#include <cstdint>
#include <unordered_map>

extern logging::Logger preprocess_logger;

using PredicateId = Index<normalized::PredicateInstantiation>;

struct PreprocessSupport {
  explicit PreprocessSupport(const normalized::Problem &problem) noexcept;
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

  void reset_tested() noexcept;

  enum class SimplifyResult { Unchanged, Changed, Invalid };

  SimplifyResult simplify(normalized::Action &action) noexcept;

  template <typename Function> void refine(Function &&select_parameters) {
    LOG_INFO(preprocess_logger, "New grounding iteration with %lu action(s)",
             std::accumulate(partially_instantiated_actions.begin(),
                             partially_instantiated_actions.end(), 0ul,
                             [](size_t sum, const auto &actions) {
                               return sum + actions.size();
                             }));

    refinement_possible = false;
    reset_tested();
    uint_fast64_t num_current_actions = 0;
    for (size_t i = 0; i < problem.actions.size(); ++i) {
      std::vector<normalized::Action> new_actions;
      for (auto &action : partially_instantiated_actions[i]) {
        if (auto result = simplify(action);
            result != SimplifyResult::Unchanged) {
          refinement_possible = true;
          if (result == SimplifyResult::Invalid) {
            num_current_actions +=
                normalized::get_num_instantiated(action, problem);
            continue;
          }
        }

        auto selection = select_parameters(action);

        if (selection.parameters.empty()) {
          new_actions.push_back(action);
          num_current_actions += 1;
          continue;
        }

        refinement_possible = true;

        for_each_action_instantiation(
            selection,
            [&](const normalized::ParameterAssignment &assignment) {
              auto new_action = ground(assignment, problem);
              if (auto result = simplify(new_action);
                  result != SimplifyResult::Invalid) {
                new_actions.push_back(std::move(new_action));
                num_current_actions += 1;
              } else {
                num_current_actions +=
                    normalized::get_num_instantiated(new_action, problem);
              }
            },
            problem);
      }
      partially_instantiated_actions[i] = std::move(new_actions);
    }
    LOG_INFO(preprocess_logger, "Progress: %d\n",
             static_cast<double>(num_current_actions) / num_actions);
  }

  bool refinement_possible = true;
  uint_fast64_t num_actions;
  std::vector<uint_fast64_t> predicate_id_offset;
  std::vector<bool> trivially_rigid;
  std::vector<bool> trivially_effectless;
  std::vector<std::vector<normalized::Action>> partially_instantiated_actions;
  std::vector<PredicateId> init;
  std::vector<std::pair<PredicateId, bool>> goal;
  mutable std::unordered_map<PredicateId, bool> rigid;
  mutable std::unordered_map<PredicateId, bool> rigid_tested;
  mutable std::unordered_map<PredicateId, bool> effectless;
  mutable std::unordered_map<PredicateId, bool> effectless_tested;
  const normalized::Problem &problem;
};

void preprocess(normalized::Problem &problem, const Config &config);

#endif /* end of include guard: PREPROCESS_HPP */
