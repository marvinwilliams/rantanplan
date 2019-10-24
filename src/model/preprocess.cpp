#include "model/model.hpp"
#include "config.hpp"
#include "model/model_utils.hpp"
#include "model/preprocess.hpp"
#include "model/support.hpp"
#include "model/to_string.hpp"
#include "logging/logging.hpp"

namespace preprocess {

logging::Logger logger = logging::Logger{"Preprocess"};

using namespace model;

void preprocess(Problem &problem, support::Support &support, const Config& config) {
  if (config.preprocess == Config::Preprocess::None) {
    LOG_INFO(logger, "No preprocessing done");
    return;
  }

  // delete unapplicable actions
  for (auto a = problem.actions.begin(); a != problem.actions.end(); ++a) {
    if (!support.simplify_action(*a)) {
      problem.actions.erase(a);
    }
  }

  bool changed_actions;
  do {
    LOG_DEBUG(logger, "New grounding iteration");
    LOG_DEBUG(logger, "Problem has %lu actions", problem.actions.size());
    changed_actions = false;
    std::vector<Action> new_actions;
    for (ActionPtr action_ptr = 0; action_ptr < problem.actions.size();
         ++action_ptr) {
      auto &action = problem.actions[action_ptr];
      LOG_DEBUG(logger, "Ground action \'%s\'", action.name.c_str());
      size_t max_pruned_actions = 0;
      size_t min_new_actions = std::numeric_limits<size_t>::max();
      const PredicateEvaluation *predicate_to_ground = nullptr;
      for (const auto &predicate : action.preconditions) {
        if (!is_grounded(predicate, action)) {
          if (max_pruned_actions > 0 && support.get_rigid(predicate.definition, !predicate.negated).empty()) {
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
          if (pruned_actions == 0 && config.preprocess == Config::Preprocess::Rigid) {
            continue;
          }
          if (pruned_actions > max_pruned_actions || (pruned_actions == max_pruned_actions && new_actions < min_new_actions)) {
            max_pruned_actions = pruned_actions;
            min_new_actions = new_actions;
            predicate_to_ground = &predicate;
          }
        }
      }
      if (config.preprocess == Config::Preprocess::Full && predicate_to_ground == nullptr) {
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
          model::to_string(*predicate_to_ground, action, problem).c_str());
      LOG_DEBUG(logger, "This predicate has %lu unfulfillable assignments", max_pruned_actions);
      LOG_DEBUG(logger, "Grounding will result in %lu new actions", min_new_actions);

      changed_actions = true;
      support.for_grounded_predicate(
          action, *predicate_to_ground,
          [&](const GroundPredicate &, support::ArgumentAssignment assignment) {
            model::Action new_action{action.name};
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
