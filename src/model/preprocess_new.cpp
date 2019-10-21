#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "model/preprocess.hpp"
#include "model/support.hpp"
#include "model/to_string.hpp"

namespace model {

void preprocess(Problem &problem, Support &support) {
  // delete unapplicable actions
  for (auto a = problem.actions.begin(); a != problem.actions.end(); ++a) {
    if (!support.simplify_action(*a)) {
      problem.actions.erase(a);
    }
  }

  bool changed_actions;
  do {
    printf("Next iteration\n");
    changed_actions = false;
    std::vector<Action> new_actions;
    for (ActionPtr action_ptr = 0; action_ptr < problem.actions.size();
         ++action_ptr) {
      auto &action = problem.actions[action_ptr];
      size_t max_pruned_actions = 0;
      size_t min_new_actions = std::numeric_limits<size_t>::max();
      const PredicateEvaluation *predicate_to_ground = nullptr;
      for (const auto &predicate : action.preconditions) {
        if (!is_grounded(predicate, action)) {
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
          /* if (pruned_actions > max_pruned_actions || (pruned_actions == max_pruned_actions && new_actions < min_new_actions)) { */
          if (pruned_actions > max_pruned_actions) {
            max_pruned_actions = pruned_actions;
            min_new_actions = new_actions;
            predicate_to_ground = &predicate;
          }
        }
      }

      if (predicate_to_ground == nullptr) {
        new_actions.push_back(std::move(action));
        continue;
      }

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

} // namespace model
