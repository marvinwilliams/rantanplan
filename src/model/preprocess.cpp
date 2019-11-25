#include "model/preprocess.hpp"
#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized_problem.hpp"
#include "model/to_string.hpp"
#include "model/utils.hpp"

#include <limits>
#include <numeric>
#include <unordered_map>
#include <unordered_set>

logging::Logger preprocess_logger = logging::Logger{"Preprocess"};

using namespace normalized;

struct PreprocessSupport {
  explicit PreprocessSupport(const Problem &problem) noexcept
      : action_assignments(problem.actions.size(), {ParameterAssignment{}}),
        /* pos_effects(problem.actions.size()), */
        /* neg_effects(problem.actions.size()), */
        pos_rigid(problem.predicates.size()),
        neg_rigid(problem.predicates.size()), problem{problem} {
    for (size_t i = 0; i < problem.predicates.size(); ++i) {
      LOG_DEBUG(logger, "Grounding predicate '%s'",
                problem.predicates[i].name.c_str());
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
      auto ground_predicate = GroundPredicate{predicate};
      initial_state[predicate.definition].push_back(get_ground_predicate_handle(
          predicate.definition, ground_predicate.arguments));
    }

    for (size_t i = 0; i < problem.actions.size(); ++i) {
      LOG_DEBUG(logger, "Grounding effects of action '%s'",
                problem.actions[i].name.c_str());
      for (const auto &predicate : problem.actions[i].effects) {
        LOG_DEBUG(logger, "Grounding effect '%s'",
                  problem.predicates[predicate.definition].name.c_str());
        for_grounded_predicate(
            problem, problem.actions[i], {}, predicate,
            [this, i, negated = predicate.negated](
                const GroundPredicate &ground_predicate,
                ParameterAssignment assignment) {
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
    LOG_DEBUG(logger, "Get rigid predicates");
    set_rigid_predicates();
  }

  GroundPredicateHandle get_ground_predicate_handle(
      PredicateHandle handle,
      const std::vector<ConstantHandle> &arguments) const {
    const PredicateDefinition &predicate = problem.predicates[handle];
    size_t factor = 1;
    size_t result = 0;
    for (size_t i = 0; i < predicate.parameters.size(); ++i) {
      result += problem.constant_index[arguments[i]] * factor;
      factor *= problem.constants_by_type[predicate.parameters[i].type].size();
    }
    return GroundPredicateHandle{result};
  }

  void set_rigid_predicates() noexcept {
    /* for (size_t i = 0; i < ground_predicates.size(); ++i) { */
    /*   GroundPredicateHandle handle{i}; */
    /*   const auto &predicate = ground_predicates[i]; */
    /*   if (pos_rigid[predicate.definition].count(handle) == 0 && */
    /*       initial_state.count(handle) > 0) { */
    /*     if (std::none_of(neg_effects.begin(), neg_effects.end(), */
    /*                      [handle](const auto &effects) { */
    /*                        return effects.count(handle) > 0; */
    /*                      })) { */
    /*       pos_rigid[predicate.definition].insert(handle); */
    /*     } */
    /*   } */
    /*   if (neg_rigid[predicate.definition].count(handle) == 0 && */
    /*       initial_state.count(handle) == 0) { */
    /*     if (std::none_of(pos_effects.begin(), pos_effects.end(), */
    /*                      [handle](const auto &effects) { */
    /*                        return effects.count(handle) > 0; */
    /*                      })) { */
    /*       neg_rigid[predicate.definition].insert(handle); */
    /*     } */
    /*   } */
    /* } */
  }

  void filter_effects(ActionHandle action_handle) {
    /* for (auto &[ground_predicate_handle, assignments] : */
    /*      pos_effects[action_handle]) { */
    /*   assignments.erase( */
    /*       std::remove_if(assignments.begin(), assignments.end(), */
    /*                      [this, action_handle](const auto &assignment) { */
    /*                        bool valid = false; */
    /*                        for (const auto &valid_assignment : */
    /*                             action_assignments[action_handle]) { */
    /*                          if (is_unifiable(valid_assignment, assignment))
     * { */
    /*                            valid = true; */
    /*                            break; */
    /*                          } */
    /*                        } */
    /*                        return !valid; */
    /*                      }), */
    /*       assignments.end()); */
    /* } */
    /* for (auto it = pos_effects[action_handle].begin(); */
    /*      it != pos_effects[action_handle].end();) { */
    /*   if (it->second.empty()) { */
    /*     it = pos_effects[action_handle].erase(it); */
    /*   } else { */
    /*     ++it; */
    /*   } */
    /* } */
    /* for (auto &[ground_predicate_handle, assignments] : */
    /*      neg_effects[action_handle]) { */
    /*   assignments.erase( */
    /*       std::remove_if(assignments.begin(), assignments.end(), */
    /*                      [this, action_handle](const auto &assignment) { */
    /*                        bool valid = false; */
    /*                        for (const auto &valid_assignment : */
    /*                             action_assignments[action_handle]) { */
    /*                          if (is_unifiable(valid_assignment, assignment))
     * { */
    /*                            valid = true; */
    /*                            break; */
    /*                          } */
    /*                        } */
    /*                        return !valid; */
    /*                      }), */
    /*       assignments.end()); */
    /* } */
    /* for (auto it = neg_effects[action_handle].begin(); */
    /*      it != neg_effects[action_handle].end();) { */
    /*   if (it->second.empty()) { */
    /*     it = neg_effects[action_handle].erase(it); */
    /*   } else { */
    /*     ++it; */
    /*   } */
    /* } */
  }

  std::vector<PredicateInstantiation> instantiations;

  std::unordered_map<PredicateInstantiation, InstantiationHandle>
      instantiation_lookup;

  std::unordered_map<PredicateHandle, std::vector<InstantiationHandle>>
      initial_state;

  std::vector<std::vector<ParameterAssignment>> action_assignments;

  std::vector<std::unordered_set<InstantiationHandle>> pos_rigid;
  std::vector<std::unordered_set<InstantiationHandle>> neg_rigid;

  const Problem &problem;
};

void preprocess(Problem &problem, const Config &config) {
  if (config.preprocess_mode == Config::PreprocessMode::None) {
    LOG_INFO(logger, "No preprocessing done");
    return;
  }

  LOG_INFO(logger, "Generating preprocessing support...");
  PreprocessSupport support{problem};

  bool changed_actions;
  bool new_iteration;
  do {
    LOG_INFO(logger, "New grounding iteration with %lu action(s)",
             std::accumulate(support.action_assignments.begin(),
                             support.action_assignments.end(), 1ul,
                             [](size_t sum, const auto &assignments) {
                               return sum + assignments.size();
                             }));

    changed_actions = false;
    new_iteration = false;
    for (size_t i = 0; i < problem.actions.size(); ++i) {
      const auto &action = problem.actions[i];
      std::vector<ParameterAssignment> new_assignments;
      bool changed_action = false;
      std::vector<size_t> order(problem.actions[i].preconditions.size());
      std::iota(order.begin(), order.end(), 0);
      if (config.preprocess_priority == Config::PreprocessPriority::Pruned) {
        std::sort(order.begin(), order.end(),
                  [&support, &action](size_t first, size_t second) {
                    const auto &first_rigid =
                        (action.preconditions[first].negated
                             ? support.pos_rigid
                             : support.neg_rigid)[action.preconditions[first]
                                                      .definition];
                    const auto &second_rigid =
                        (action.preconditions[second].negated
                             ? support.pos_rigid
                             : support.neg_rigid)[action.preconditions[second]
                                                      .definition];
                    return first_rigid.size() < second_rigid.size();
                  });
      }
      for (const auto &assignment : support.action_assignments[i]) {
        bool valid = true;
        for (const auto &predicate : action.preconditions) {
          if (is_grounded(assignment, predicate)) {
            auto ground_predicate =
                GroundPredicate{problem, action, assignment, predicate};
            auto ground_predicate_handle =
                support.ground_predicate_lookup.at(ground_predicate);
            if ((predicate.negated ? support.pos_rigid
                                   : support.neg_rigid)[predicate.definition]
                    .count(ground_predicate_handle) > 0) {
              valid = false;
              break;
            }
          }
        }
        if (!valid) {
          changed_action = true;
          new_iteration = true;
          continue;
        }
        if (config.preprocess_priority == Config::PreprocessPriority::New) {
          std::sort(
              order.begin(), order.end(),
              [&problem, &action, &assignment](size_t first, size_t second) {
                return get_num_grounded(problem, action, assignment,
                                        action.preconditions[first]) <
                       get_num_grounded(problem, action, assignment,
                                        action.preconditions[second]);
              });
        }
        size_t max_num_pruned = 0;
        size_t min_num_new = std::numeric_limits<size_t>::max();
        const PredicateEvaluation *predicate_to_ground = nullptr;
        for (size_t p : order) {
          const auto &predicate = action.preconditions[p];
          size_t num_pruned = 0;
          size_t num_new = 0;
          if (config.preprocess_priority ==
                  Config::PreprocessPriority::Pruned &&
              (predicate.negated ? support.pos_rigid
                                 : support.neg_rigid)[predicate.definition]
                      .size() < max_num_pruned) {
            continue;
          }
          if (config.preprocess_priority == Config::PreprocessPriority::New &&
              get_num_grounded(problem, action, assignment, predicate) -
                      (predicate.negated
                           ? support.pos_rigid
                           : support.neg_rigid)[predicate.definition]
                          .size() >
                  min_num_new) {
            continue;
          }
          for_grounded_predicate(
              problem, action, assignment, predicate,
              [&support, &predicate, &num_pruned,
               &num_new](const GroundPredicate &ground_predicate,
                         const ParameterAssignment &) {
                GroundPredicateHandle handle =
                    support.ground_predicate_lookup.at(ground_predicate);
                if ((predicate.negated
                         ? support.pos_rigid
                         : support.neg_rigid)[predicate.definition]
                        .count(handle) == 0) {
                  ++num_new;
                } else {
                  ++num_pruned;
                }
              });
          if (num_new == 0) {
            valid = false;
            break;
          }
          if (is_grounded(assignment, predicate)) {
            continue;
          }
          if (num_pruned == 0 &&
              config.preprocess_mode == Config::PreprocessMode::Rigid) {
            continue;
          }
          if (config.preprocess_priority ==
              Config::PreprocessPriority::Pruned) {
            if (num_pruned > max_num_pruned ||
                (num_pruned == max_num_pruned && num_new < min_num_new)) {
              max_num_pruned = num_pruned;
              min_num_new = num_new;
              predicate_to_ground = &predicate;
            }
          } else if (config.preprocess_priority ==
                     Config::PreprocessPriority::New) {
            if (num_new < min_num_new ||
                (num_new == min_num_new && num_pruned > max_num_pruned)) {
              max_num_pruned = num_pruned;
              min_num_new = num_new;
              predicate_to_ground = &predicate;
            }
          } else {
            assert(false);
          }
        }

        if (!valid) {
          changed_action = true;
          new_iteration = true;
          continue;
        }

        if (config.preprocess_mode == Config::PreprocessMode::Full &&
            predicate_to_ground == nullptr) {
          for (const auto &predicate : action.effects) {
            if (!is_grounded(assignment, predicate)) {
              predicate_to_ground = &predicate;
              break;
            }
          }
        }

        if (predicate_to_ground == nullptr) {
          new_assignments.push_back(assignment);
          continue;
        }

        new_iteration = true;
        if (max_num_pruned > 0) {
          changed_action = true;
        }
        for_grounded_predicate(
            problem, action, assignment, *predicate_to_ground,
            [&](const GroundPredicate &ground_predicate,
                ParameterAssignment predicate_assignment) {
              GroundPredicateHandle handle =
                  support.ground_predicate_lookup.at(ground_predicate);
              if ((predicate_to_ground->negated
                       ? support.pos_rigid
                       : support.neg_rigid)[predicate_to_ground->definition]
                      .count(handle) == 0) {
                predicate_assignment.insert(assignment.begin(),
                                            assignment.end());
                new_assignments.push_back(std::move(predicate_assignment));
              }
            });
      }
      support.action_assignments[i] = new_assignments;
      if (changed_action) {
        changed_actions = true;
        support.filter_effects(ActionHandle{i});
      }
    }
    if (changed_actions) {
      LOG_DEBUG(logger, "Updating rigid predicates");
      support.set_rigid_predicates();
    }
  } while (new_iteration);

  for (size_t i = 0; i < problem.actions.size(); ++i) {
    for (const auto &assignment : support.action_assignments[i]) {
      problem.action_groundings.push_back({ActionHandle{i}, assignment});
    }
  }
}
