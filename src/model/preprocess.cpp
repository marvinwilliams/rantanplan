#include "model/preprocess.hpp"
#include "model/model.hpp"
#include "model/support.hpp"
#include "model/to_string.hpp"

namespace model {

void ground_rigid(Problem &problem, Support &support) noexcept {
  bool found_rigid;
  do {
    found_rigid = false;
    std::vector<model::Action> new_actions;
    for (const model::Action &action : problem.actions) {
      size_t min_grounded_size = std::numeric_limits<size_t>::max();
      std::vector<size_t> to_ground;
      for (const auto &predicate : action.preconditions) {
        if (support.is_rigid(predicate.definition)) {
          support::ArgumentMapping mapping(action.parameters,
                                           predicate.arguments);
          size_t grounded_size = std::accumulate(
              mapping.matches.begin(), mapping.matches.end(), 1ul,
              [&action, &support](size_t product, const auto &m) {
                return product * support
                                     .get_constants_of_type(
                                         action.parameters[m.first].type)
                                     .size();
              });
          auto in_init = static_cast<size_t>(std::count_if(
              problem.initial_state.begin(), problem.initial_state.end(),
              [&predicate](const auto &p) {
                return p.definition == predicate.definition;
              }));
          if (predicate.negated) {
            /* if (1 - (static_cast<float>(in_init) / */
            /*          static_cast<float>(grounded_size)) <= */
            /*     ratio) { */
            if (grounded_size - in_init < min_grounded_size) {
              min_grounded_size = grounded_size - in_init;
              to_ground.clear();
              to_ground.reserve(mapping.matches.size());
              std::transform(mapping.matches.begin(), mapping.matches.end(),
                             std::back_inserter(to_ground),
                             [](const auto &m) { return m.first; });
            }
            /* } */
          } else {
            /* if (static_cast<float>(in_init) / */
            /*         static_cast<float>(grounded_size) <= */
            /*     ratio) { */
            if (in_init < min_grounded_size) {
              min_grounded_size = in_init;
              to_ground.clear();
              to_ground.reserve(mapping.matches.size());
              std::transform(mapping.matches.begin(), mapping.matches.end(),
                             std::back_inserter(to_ground),
                             [](const auto &m) { return m.first; });
            }
            /* } */
          }
        }
      }
      if (min_grounded_size == std::numeric_limits<size_t>::max()) {
        new_actions.push_back(action);
        continue;
      }
      found_rigid = true;

      std::vector<size_t> number_arguments;
      number_arguments.reserve(to_ground.size());
      for (auto index : to_ground) {
        auto type = action.parameters[index].type;
        number_arguments.push_back(support.get_constants_of_type(type).size());
      }

      auto combination_iterator = CombinationIterator{number_arguments};

      while (!combination_iterator.end()) {
        const auto &combination = *combination_iterator;
        model::Action new_action{action.name};
        for (size_t i = 0; i < to_ground.size(); ++i) {
          auto type = action.parameters[to_ground[i]].type;
          new_action.parameters.(to_ground[i], support.get_constants_of_type(
                                                   type)[combination[i]]);
        }
        bool valid = true;
        for (const auto &predicate : action.preconditions) {
          model::PredicateEvaluation new_predicate = predicate;
          for (size_t i = 0; i < to_ground.size(); ++i) {
            for (size_t j = 0; j < new_predicate.arguments.size(); ++j) {
              auto parameter_ptr =
                  std::get_if<ParameterPtr>(&new_predicate.arguments[j]);
              if (parameter_ptr && *parameter_ptr == to_ground[i]) {
                new_predicate.arguments[j] = new_action.arguments[i].second;
              }
            }
          }
          if (support.is_grounded(new_predicate)) {
            model::GroundPredicate ground_predicate{new_predicate};
            if (support.is_rigid(ground_predicate, predicate.negated)) {
              continue;
            } else if (support.is_rigid(ground_predicate, !predicate.negated)) {
              valid = false;
              break;
            }
          }
          new_action.preconditions.push_back(std::move(new_predicate));
        }
        if (!valid) {
          ++combination_iterator;
          continue;
        }
        for (const auto &predicate : action.effects) {
          model::PredicateEvaluation new_predicate = predicate;
          for (size_t i = 0; i < to_ground.size(); ++i) {
            for (size_t j = 0; j < new_predicate.arguments.size(); ++j) {
              auto parameter_ptr =
                  std::get_if<ParameterPtr>(&new_predicate.arguments[j]);
              if (parameter_ptr && *parameter_ptr == to_ground[i]) {
                new_predicate.arguments[j] = new_action.arguments[i].second;
              }
            }
          }
          new_action.effects.push_back(std::move(new_predicate));
        }
        new_action.parameters = action.parameters;
        new_action.arguments.insert(new_action.arguments.end(),
                                    action.arguments.begin(),
                                    action.arguments.end());
        /* PRINT_DEBUG("Added action %s", */
        /*             model::to_string(new_action, problem).c_str()); */
        new_actions.push_back(std::move(new_action));
        ++combination_iterator;
      }
    }
    problem.actions = std::move(new_actions);
    /* PRINT_DEBUG("This iteration:\n%s", model::to_string(problem).c_str()); */
    support.update();
  } while (found_rigid);
}

void preprocess(Problem &problem, Support &support) {
  for (auto a = problem.actions.begin(); a != problem.actions.end(); ++a) {
    if (!support.simplify_action(*a)) {
      problem.actions.erase(a);
    }
  }
  ground_rigid(problem, support);
}

} // namespace model
