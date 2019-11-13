#include "config.hpp"
#include "logging/logging.hpp"
#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "model/preprocess.hpp"
#include "model/to_string.hpp"

#include <limits>
#include <numeric>
#include <unordered_map>
#include <unordered_set>

namespace preprocess {

logging::Logger logger = logging::Logger{"Preprocess"};

using namespace model;

struct PreprocessSupport {
  explicit PreprocessSupport(const Problem &problem) noexcept
      : pos_rigid(problem.predicates.size()),
        neg_rigid(problem.predicates.size()),
        pos_tested(problem.predicates.size()),
        neg_tested(problem.predicates.size()), problem{problem} {

    trivially_rigid.reserve(problem.predicates.size());
    for (size_t i = 0; i < problem.predicates.size(); ++i) {
      trivially_rigid.push_back(std::none_of(
          problem.actions.begin(), problem.actions.end(),
          [i](const auto &action) {
            return std::any_of(action.effects.begin(), action.effects.end(),
                               [i](const auto &predicate) {
                                 return predicate.definition ==
                                        PredicateHandle{i};
                               });
          }));
    }

    action_assignments.reserve(problem.actions.size());
    for (const auto &action : problem.actions) {
      action_assignments.push_back(
          {ParameterAssignment(action.parameters.size())});
    }

    initial_state.resize(problem.predicates.size());
    for (const PredicateEvaluation &predicate : problem.initial_state) {
      assert(!predicate.negated);
      auto ground_predicate = GroundPredicate{predicate};
      initial_state[predicate.definition].insert(get_ground_predicate_handle(
          problem, predicate.definition, ground_predicate.arguments));
    }
  }

  bool is_trivially_rigid(const GroundPredicate &predicate,
                          bool negated) const {
    auto handle = get_ground_predicate_handle(problem, predicate.definition,
                                              predicate.arguments);
    if ((initial_state[predicate.definition].count(handle) > 0) == negated) {
      return false;
    }
    return trivially_rigid[predicate.definition];
  }

  bool is_rigid(const GroundPredicate &predicate, bool negated) const {
    auto handle = get_ground_predicate_handle(problem, predicate.definition,
                                              predicate.arguments);
    auto &rigid = (negated ? neg_rigid : pos_rigid)[predicate.definition];
    auto &tested = (negated ? neg_tested : pos_tested)[predicate.definition];
    if (rigid.count(handle)) {
      return true;
    }
    if (tested.count(handle)) {
      return false;
    }
    tested.insert(handle);
    bool is_initial = initial_state[predicate.definition].count(handle) > 0;
    if (is_initial == negated) {
      return false;
    }
    if (trivially_rigid[predicate.definition]) {
      rigid.insert(handle);
      return true;
    }
    for (size_t i = 0; i < problem.actions.size(); ++i) {
      const auto &action = problem.actions[i];
      for (const auto &effect : action.effects) {
        if (effect.definition == predicate.definition &&
            effect.negated != negated) {
          if (is_groundable(problem, action,
                            ParameterAssignment(action.parameters.size()),
                            effect, predicate.arguments)) {
            for (const auto &assignment : action_assignments[i]) {
              if (is_groundable(problem, action, assignment, effect,
                                predicate.arguments)) {
                return false;
              }
            }
          }
        }
      }
    }
    rigid.insert(handle);
    return true;
  }

  template <typename Function> void refine(Function &&best_predicate) {
    LOG_INFO(logger, "New grounding iteration with %lu action(s)",
             std::accumulate(action_assignments.begin(),
                             action_assignments.end(), 0ul,
                             [](size_t sum, const auto &assignments) {
                               return sum + assignments.size();
                             }));

    refinement_possible = false;
    std::for_each(pos_tested.begin(), pos_tested.end(),
                  [](auto &t) { t.clear(); });
    std::for_each(neg_tested.begin(), neg_tested.end(),
                  [](auto &t) { t.clear(); });
    for (size_t i = 0; i < problem.actions.size(); ++i) {
      const auto &action = problem.actions[i];
      std::vector<ParameterAssignment> new_assignments;
      for (size_t j = 0; j < action_assignments[i].size(); ++j) {
        const auto &assignment = action_assignments[i][j];
        bool valid = true;
        std::vector<size_t> to_check;
        for (size_t k = 0; k < action.preconditions.size(); ++k) {
          const auto &predicate = action.preconditions[k];
          if (is_grounded(assignment, predicate)) {
            auto ground_predicate =
                GroundPredicate{problem, action, assignment, predicate};
            if (is_rigid(ground_predicate, !predicate.negated)) {
              valid = false;
              break;
            }
          } else {
            to_check.push_back(k);
          }
        }

        if (!valid) {
          refinement_possible = true;
          continue;
        }

        if (to_check.empty()) {
          new_assignments.push_back(assignment);
          continue;
        }

        size_t min_value = std::numeric_limits<size_t>::max();
        const PredicateEvaluation *predicate_to_ground = nullptr;

        for (size_t k : to_check) {
          const auto &predicate = action.preconditions[k];
          size_t value = best_predicate(action, assignment, predicate);
          if (value < min_value) {
            min_value = value;
            predicate_to_ground = &predicate;
          }
        }

        /* for (size_t k : to_check) { */
        /*   const auto &predicate = action.preconditions[k]; */
        /*   size_t num_new = */
        /*       get_num_grounded(problem, action, assignment, predicate); */
        /*   if (num_new < min_num_new) { */
        /*     min_num_new = num_new; */
        /*     predicate_to_ground = &predicate; */
        /*   } */
        /* } */

        assert(predicate_to_ground);

        refinement_possible = true;

        for_grounded_predicate(
            problem, action, assignment, *predicate_to_ground,
            [&](ParameterAssignment predicate_assignment) {
              for (size_t k = 0; k < action.parameters.size(); ++k) {
                if (assignment[k]) {
                  assert(!predicate_assignment[k]);
                  predicate_assignment[k] = *assignment[k];
                }
              }
              for (size_t k = 0; k < action.preconditions.size(); ++k) {
                const auto &predicate = action.preconditions[k];
                if (is_grounded(assignment, predicate)) {
                  auto ground_predicate =
                      GroundPredicate{problem, action, assignment, predicate};
                  if (is_rigid(ground_predicate, !predicate.negated)) {
                    valid = false;
                    break;
                  }
                }
              }
              if (valid) {
                new_assignments.push_back(std::move(predicate_assignment));
              }
            });
      }
      action_assignments[i] = std::move(new_assignments);
    }
  }

  std::vector<
      std::unordered_set<GroundPredicateHandle, hash::Handle<GroundPredicate>>>
      initial_state;

  bool refinement_possible = true;
  std::vector<std::vector<ParameterAssignment>> action_assignments;
  std::vector<bool> trivially_rigid;
  mutable std::vector<
      std::unordered_set<GroundPredicateHandle, hash::Handle<GroundPredicate>>>
      pos_rigid;
  mutable std::vector<
      std::unordered_set<GroundPredicateHandle, hash::Handle<GroundPredicate>>>
      neg_rigid;
  mutable std::vector<
      std::unordered_set<GroundPredicateHandle, hash::Handle<GroundPredicate>>>
      pos_tested;
  mutable std::vector<
      std::unordered_set<GroundPredicateHandle, hash::Handle<GroundPredicate>>>
      neg_tested;
  const Problem &problem;
};

void preprocess(Problem &problem, const Config &config) {
  if (config.preprocess_mode == Config::PreprocessMode::None) {
    LOG_INFO(logger, "No preprocessing done");
    return;
  }

  LOG_INFO(logger, "Generating preprocessing support...");
  PreprocessSupport support{problem};

  while (support.refinement_possible) {
    support.refine([&problem](const auto &action, const auto &assignment,
                              const auto &predicate) {
      return get_num_grounded(problem, action, assignment, predicate);
    });
  }

  for (size_t i = 0; i < problem.actions.size(); ++i) {
    for (const auto &assignment : support.action_assignments[i]) {
      problem.action_groundings.push_back({ActionHandle{i}, assignment});
    }
  }
}

} // namespace preprocess
