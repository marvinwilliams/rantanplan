#include "model/preprocess.hpp"
#include "config.hpp"
#include "logging/logging.hpp"
#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "model/support.hpp"
#include "model/to_string.hpp"

namespace preprocess {

logging::Logger logger = logging::Logger{"Preprocess"};

using namespace model;

struct PreprocessSupport {
  explicit PreprocessSupport(const Problem &problem) noexcept
      : action_assignments(problem.actions.size(), {ArgumentAssignment{}}),
        pos_effects(problem.actions.size()),
        neg_effects(problem.actions.size()), problem{problem} {
    for (size_t i = 0; i < problem.predicates.size(); ++i) {
      for_grounded_predicate(
          problem, PredicateHandle{i},
          [this](const GroundPredicate &ground_predicate) {
            ground_predicates.push_back(ground_predicate);
            ground_predicate_lookup.insert(
                {std::move(ground_predicate),
                 GroundPredicateHandle{ground_predicate_lookup.size()}});
          });
    }

    initial_state.reserve(problem.initial_state.size());
    for (const PredicateEvaluation &predicate : problem.initial_state) {
      assert(!predicate.negated);
      assert(ground_predicates.count(predicate) > 0);
      initial_state.insert(ground_predicate_lookup[predicate]);
    }

    for (size_t i = 0; i < problem.actions.size(); ++i) {
      for (const auto &predicate : problem.actions[i].effects) {
        for_grounded_predicate(
            problem, ActionHandle{i}, predicate,
            [this, i, negated = predicate.negated](
                const GroundPredicate &ground_predicate,
                ArgumentAssignment assignment) {
              auto ground_predicate_ptr =
                  ground_predicate_lookup[ground_predicate];
              if (negated) {
                neg_effects[i][ground_predicate_ptr].push_back(
                    std::move(assignment));
              } else {
                pos_effects[i][ground_predicate_ptr].push_back(
                    std::move(assignment));
              }
            });
      }
    }

    set_rigid_predicates();
  }

  void set_rigid_predicates() noexcept {
    for (size_t i = 0; i < ground_predicates.size(); ++i) {
      GroundPredicateHandle handle{i};
      if (pos_rigid.count(handle) == 0 && initial_state.count(handle) > 0) {
        if (std::none_of(neg_effects.begin(), neg_effects.end(),
                         [handle](const auto &effects) {
                           return effects.count(handle) > 0;
                         })) {
          pos_rigid.insert(handle);
        }
      }
      if (neg_rigid.count(handle) == 0 && initial_state.count(handle) == 0) {
        if (std::none_of(pos_effects.begin(), pos_effects.end(),
                         [handle](const auto &effects) {
                           return effects.count(handle) > 0;
                         })) {
          neg_rigid.insert(handle);
        }
      }
    }
  }

  void update(const Problem &problem) {
    // Filter support
    for (size_t i = 0; i < problem.actions.size(); ++i) {
      for (auto &[ground_predicate_handle, assignments] : pos_effects[i]) {
        assignments.erase(
            std::remove_if(assignments.begin(), assignments.end(),
                           [this, i](const auto &assignment) {
                             bool valid = false;
                             for (const auto &valid_assignment :
                                  action_assignments[i]) {
                               if (is_unifiable(valid_assignment, assignment)) {
                                 valid = true;
                                 break;
                               }
                             }
                             return !valid;
                           }),
            assignments.end());
      }
      pos_effects[i].erase(
          std::remove_if(pos_effects[i].begin(), pos_effects[i].end(),
                         [](const auto &action_support) {
                           return action_support.second.empty();
                         }),
          pos_effects[i].end());
      for (auto &[ground_predicate_handle, assignments] : neg_effects[i]) {
        assignments.erase(
            std::remove_if(assignments.begin(), assignments.end(),
                           [this, i](const auto &assignment) {
                             bool valid = false;
                             for (const auto &valid_assignment :
                                  action_assignments[i]) {
                               if (is_unifiable(valid_assignment, assignment)) {
                                 valid = true;
                                 break;
                               }
                             }
                             return !valid;
                           }),
            assignments.end());
      }
      neg_effects[i].erase(
          std::remove_if(neg_effects[i].begin(), neg_effects[i].end(),
                         [](const auto &action_support) {
                           return action_support.second.empty();
                         }),
          neg_effects[i].end());
    }
    set_rigid_predicates();
  }

  std::vector<GroundPredicate> ground_predicates;

  std::unordered_map<GroundPredicate, GroundPredicateHandle,
                     hash::GroundPredicate>
      ground_predicate_lookup;

  std::unordered_set<GroundPredicateHandle, hash::Handle<GroundPredicate>>
      initial_state;

  std::vector<std::vector<ArgumentAssignment>> action_assignments;

  std::vector<
      std::unordered_map<GroundPredicateHandle, std::vector<ArgumentAssignment>,
                         hash::Handle<GroundPredicate>>>
      pos_effects;
  std::vector<
      std::unordered_map<GroundPredicateHandle, std::vector<ArgumentAssignment>,
                         hash::Handle<GroundPredicate>>>
      neg_effects;

  std::unordered_set<GroundPredicateHandle, hash::Handle<GroundPredicate>>
      pos_rigid;
  std::unordered_set<GroundPredicateHandle, hash::Handle<GroundPredicate>>
      neg_rigid;

  const Problem &problem;
};

void preprocess(Problem &problem, const Config &config) {
  if (config.preprocess == Config::Preprocess::None) {
    LOG_INFO(logger, "No preprocessing done");
    return;
  }

  PreprocessSupport support{problem};

  bool changed_actions;
  do {
    LOG_DEBUG(logger, "New grounding iteration");
    LOG_DEBUG(logger, "Problem has %lu actions",
              std::accumulate(support.action_assignments.begin(),
                              support.action_assignments.end(), 1ul,
                              [](size_t sum, const auto &assignments) {
                                return sum + assignments.size();
                              }));

    changed_actions = false;
    for (size_t i = 0; i < problem.actions.size(); ++i) {
      const auto &action = problem.actions[i];
      std::vector<ArgumentAssignment> new_assignments;
      for (const auto &assignment : support.action_assignments[i]) {
        for (const auto &predicate : action.preconditions) {
          const auto &rigid_predicates =
              predicate.negated ? support.pos_rigid
                                : support.neg_rigid;
          size_t num_pruned = 0;
          size_t num_new = 0;
          for_grounded_predicate(problem, ActionHandle{i}, assignment, predicate,
                                 [&](const GroundPredicate &ground_predicate,
                                     const ArgumentAssignment &) {
                                   auto handle =
                                       support.ground_predicate_lookup.at(
                                           ground_predicate);
                                   if (rigid_predicates.count(handle) == 0) {
                                     ++num_new;
                                   } else {
                                     ++num_pruned;
                                   }
                                 });
        }
      }

      if (!support.simplify_action(action)) {
        LOG_DEBUG(logger, "Action \'%s\' became unapplicable",
                  action.name.c_str());
        changed_actions = true;
        continue;
      }
      LOG_DEBUG(logger, "Ground action \'%s\'", action.name.c_str());
      size_t max_pruned_actions = 0;
      size_t min_new_actions = std::numeric_limits<size_t>::max();
      const PredicateEvaluation *predicate_to_ground = nullptr;
      for (const auto &predicate : action.preconditions) {
        if (!is_grounded(predicate, action)) {
          if (max_pruned_actions > 0 &&
              support.get_rigid(predicate.definition, !predicate.negated)
                  .empty()) {
            continue;
          }
          size_t new_actions = 0;
          size_t pruned_actions = 0;
          support.for_grounded_predicate(
              action, predicate,
              [&](const GroundPredicate &ground_predicate,
                  const support::ArgumentAssignment &) {
                if (support.is_rigid(ground_predicate, !predicate.negated)) {
                  ++pruned_actions;
                } else {
                  ++new_actions;
                }
              });
          if (pruned_actions == 0 &&
              config.preprocess == Config::Preprocess::Rigid) {
            continue;
          }
          if (pruned_actions > max_pruned_actions ||
              (pruned_actions == max_pruned_actions &&
               new_actions < min_new_actions)) {
            max_pruned_actions = pruned_actions;
            min_new_actions = new_actions;
            predicate_to_ground = &predicate;
          }
        }
      }
      if (config.preprocess == Config::Preprocess::Full &&
          predicate_to_ground == nullptr) {
        for (const auto &predicate : action.effects) {
          if (!is_grounded(predicate, action)) {
            predicate_to_ground = &predicate;
            break;
          }
        }
      }

      if (predicate_to_ground == nullptr) {
        LOG_DEBUG(logger, "No predicate can be grounded");
        new_actions.push_back(std::move(action));
        continue;
      }
      LOG_DEBUG(logger, "Grounding predicate \'%s\'",
                to_string(*predicate_to_ground, action, problem).c_str());
      LOG_DEBUG(logger, "This predicate has %lu unfulfillable assignments",
                max_pruned_actions);
      LOG_DEBUG(logger, "Grounding will result in %lu new actions",
                min_new_actions);

      changed_actions = true;
      support.for_grounded_predicate(
          action, *predicate_to_ground,
          [&](const GroundPredicate &, support::ArgumentAssignment assignment) {
            Action new_action{action.name};
            new_action.parameters = action.parameters;
            for (const auto &[parameter_pos, constant_index] :
                 assignment.arguments) {
              new_action.parameters[parameter_pos]
                  .constant = support.get_constants_of_type(
                  new_action.parameters[parameter_pos].type)[constant_index];
            }
            new_action.preconditions = action.preconditions;
            new_action.effects = action.effects;
            if (support.simplify_action(new_action)) {
              new_actions.push_back(std::move(new_action));
            }
          });
    }
    problem.actions = std::move(new_actions);
    support.update();
  } while (changed_actions);
}

} // namespace preprocess
