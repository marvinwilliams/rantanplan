#include "model/support.hpp"
#include "logging/logging.hpp"
#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "util/combinatorics.hpp"

#include <algorithm>
#include <cassert>
#include <variant>

namespace support {

logging::Logger logger{"Support"};

using namespace model;

Support::Support(const Problem &problem_) noexcept : problem_{problem_} {
  initial_state_.reserve(problem_.initial_state.size());
  for (const PredicateEvaluation &predicate : problem_.initial_state) {
    assert(!predicate.negated);
    initial_state_.emplace(predicate);
  }
  LOG_DEBUG(logger, "Sort constants by type...");
  LOG_DEBUG(logger, "Ground all predicates...");
  ground_predicates();
  ground_predicates_constructed_ = true;
  LOG_DEBUG(logger, "The problem_ has %u grounded predicates",
            std::accumulate(
                ground_predicates_.begin(), ground_predicates_.end(), 0,
                [](size_t sum, const auto &p) { return sum + p.size(); }));
  LOG_DEBUG(logger, "Compute predicate support...");
  set_predicate_support();
  predicate_support_constructed_ = true;
  /* for (size_t i = 0; i < problem_.predicates.size(); ++i) { */
  /*   LOG_DEBUG(logger, "%s %s\n", */
  /*             model::to_string(problem_.predicates[i], problem_).c_str(), */
  /*             is_rigid(i) ? "rigid" : "not rigid"); */
  /* } */
  LOG_DEBUG(logger, "");
  if constexpr (logging::has_debug_log()) {
    for (const auto &predicate : ground_predicates_) {
      LOG_DEBUG(logger, "%s %s\n",
                model::to_string(predicate.first, problem_).c_str(),
                is_rigid(predicate.first, false) ? "rigid" : "not rigid");
      LOG_DEBUG(logger, "!%s %s\n",
                model::to_string(predicate.first, problem_).c_str(),
                is_rigid(predicate.first, true) ? "rigid" : "not rigid");
    }
  }
}

void Support::update() noexcept {
  pos_precondition_support_.clear();
  neg_precondition_support_.clear();
  pos_effect_support_.clear();
  neg_effect_support_.clear();
  pos_rigid_predicates_.clear();
  neg_rigid_predicates_.clear();
  set_predicate_support();
}

void Support::ground_predicates() noexcept {
  for (size_t i = 0; i < problem_.predicates.size(); ++i) {
    for_grounded_predicate(
        PredicateHandle{i}, [this](GroundPredicate ground_predicate) {
          ground_predicates_.insert(
              {std::move(ground_predicate),
               GroundPredicateHandle{ground_predicates_.size()}});
        });
  }
}

void Support::set_predicate_support() noexcept {
  pos_precondition_support_.resize(ground_predicates_.size());
  neg_precondition_support_.resize(ground_predicates_.size());
  pos_effect_support_.resize(ground_predicates_.size());
  neg_effect_support_.resize(ground_predicates_.size());

  for (size_t i = 0; i < problem_.actions.size(); ++i) {
    const auto &action = problem_.actions[i];
    for (const auto &predicate : action.preconditions) {
      for_grounded_predicate(
          action, predicate,
          [this, negated = predicate.negated, action_handle = ActionHandle{i}](
              GroundPredicateHandle ground_predicate_handle,
              ArgumentAssignment assignment) {
            auto &predicate_support = select_support(negated, false);
            predicate_support[ground_predicate_handle][action_handle].push_back(
                std::move(assignment));
          });
    }
    for (const auto &predicate : action.effects) {
      for_grounded_predicate(
          action, predicate,
          [this, negated = predicate.negated, action_handle = ActionHandle{i}](
              GroundPredicateHandle ground_predicate_handle,
              ArgumentAssignment assignment) {
            auto &predicate_support = select_support(negated, false);
            predicate_support[ground_predicate_handle][action_handle].push_back(
                std::move(assignment));
          });
    }
  }

  for (const auto &[ground_predicate, ground_predicate_handle] :
       ground_predicates_) {
    if (neg_effect_support_[ground_predicate_handle].empty() &&
        is_init(ground_predicate_handle)) {
      pos_rigid_predicates_.insert(ground_predicate_handle);
    }
    if (pos_effect_support_[ground_predicate_handle].empty() &&
        !is_init(ground_predicate_handle)) {
      neg_rigid_predicates_.insert(ground_predicate_handle);
    }
  }
}

bool Support::simplify_action(model::Action &action) noexcept {
  bool valid = true;
  auto end =
      std::remove_if(action.preconditions.begin(), action.preconditions.end(),
                     [&](const auto &p) {
                       if (is_grounded(p, action)) {
                         model::GroundPredicate ground_predicate{p, action};
                         if (is_rigid(ground_predicate, p.negated)) {
                           return true;
                         } else if (is_rigid(ground_predicate, !p.negated)) {
                           valid = false;
                         }
                       }
                       return false;
                     });
  if (!valid) {
    return false;
  }
  action.preconditions.erase(end, action.preconditions.end());
  return true;
}

} // namespace support
