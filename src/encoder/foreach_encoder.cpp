#include "encoder/foreach_encoder.hpp"
#include "config.hpp"
#include "encoder/encoder.hpp"
#include "encoder/support.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "planner/planner.hpp"
#include "sat/formula.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <vector>

using namespace normalized;

ForeachEncoder::ForeachEncoder(const std::shared_ptr<Problem> &problem) noexcept
    : Encoder{problem}, support_{*problem} {
  if (!support_.is_initialized()) {
    return;
  }
  LOG_INFO(encoding_logger, "Init sat variables...");
  if (!init_sat_vars()) {
    return;
  }
  LOG_INFO(encoding_logger, "Encode problem...");
  if (!encode_init()) {
    return;
  }
  if (!encode_actions()) {
    return;
  }
  if (!parameter_implies_predicate()) {
    return;
  }
  if (!interference()) {
    return;
  }
  if (!frame_axioms()) {
    return;
  }
  if (!assume_goal()) {
    return;
  }
  num_vars_ -= 3; // subtract SAT und UNSAT for correct step semantics
  initialized_ = true;
  LOG_INFO(encoding_logger, "Variables per step: %u", num_vars_);
  LOG_INFO(encoding_logger, "Helper variables to mitigate dnf explosion: %u",
           num_helpers_);
  LOG_INFO(encoding_logger, "Init clauses: %u", init_.clauses.size());
  LOG_INFO(encoding_logger, "Universal clauses: %u",
           universal_clauses_.clauses.size());
  LOG_INFO(encoding_logger, "Transition clauses: %u",
           transition_clauses_.clauses.size());
  LOG_INFO(encoding_logger, "Goal clauses: %u", goal_.clauses.size());
}

int ForeachEncoder::to_sat_var(Literal l, unsigned int step) const noexcept {
  uint_fast64_t variable = 0;
  variable = l.variable.sat_var;
  if (variable == DONTCARE) {
    return static_cast<int>(SAT);
  }
  if (variable == SAT || variable == UNSAT) {
    return (l.positive ? 1 : -1) * static_cast<int>(variable);
  }
  step += l.variable.this_step ? 0 : 1;
  return (l.positive ? 1 : -1) * static_cast<int>(variable + step * num_vars_);
}

Plan ForeachEncoder::extract_plan(const sat::Model &model,
                                  unsigned int step) const noexcept {
  Plan plan;
  plan.problem = problem_;
  for (unsigned int s = 0; s < step; ++s) {
    for (size_t i = 0; i < problem_->actions.size(); ++i) {
      if (!model[actions_[i] + s * num_vars_]) {
        continue;
      }
      const Action &action = problem_->actions[i];
      std::vector<ConstantIndex> constants(action.parameters.size());
      for (size_t j = 0; j < action.parameters.size(); ++j) {
        if (action.parameters[j].is_constant()) {
          constants[j] = action.parameters[j].get_constant();
        }
      }
      for (size_t selection_index = 0; selection_index < selections_[i].size();
           ++selection_index) {
        for (const auto &selection : selections_[i])
          for (auto it = AssignmentIterator{selection, action, *problem_};
               it != AssignmentIterator{}; ++it) {
            assert(assignments_[i][selection_index].find(
                       get_assignment_id(*it, *problem_)) !=
                   assignments_[i][selection_index].end());
            if (model[assignments_[i][selection_index].at(
                          get_assignment_id(*it, *problem_)) +
                      s * num_vars_]) {
              for (auto [index, c] : *it) {
                constants[index] = c;
              }
              break;
            }
          }
      }
      plan.sequence.emplace_back(ActionIndex{i}, std::move(constants));
    }
  }
  return plan;
}

size_t ForeachEncoder::get_assignment_id(const ParameterAssignment &assignment,
                                         const Problem &problem) const
    noexcept {
  size_t result = 0;
  for (auto [index, constant] : assignment) {
    result = (result * problem.constants.size()) + constant;
  }
  return result;
}

size_t ForeachEncoder::get_selection_index(
    size_t action_index,
    const normalized::ParameterAssignment &assignment) const noexcept {
  size_t selection_index = 0;
  for (; selection_index < selections_[action_index].size();
       ++selection_index) {
    const auto &selection = selections_[action_index][selection_index];
    if (selection.size() != assignment.size()) {
      continue;
    }
    if (std::mismatch(selection.begin(), selection.end(), assignment.begin(),
                      assignment.end(),
                      [](auto s, auto a) { return s == a.first; })
            .first == selection.end()) {
      break;
    }
  }
  assert(selection_index < selections_[action_index].size());
  return selection_index;
}

bool ForeachEncoder::init_sat_vars() noexcept {
  actions_.reserve(problem_->actions.size());
  selections_.resize(problem_->actions.size());
  assignments_.resize(problem_->actions.size());
  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    const auto &action = problem_->actions[i];
    actions_.push_back(num_vars_++);

    for (const auto &predicate : action.preconditions) {
      auto selection = get_referenced_parameters(action, predicate);
      if (std::find(selections_[i].begin(), selections_[i].end(), selection) ==
          selections_[i].end()) {
        selections_[i].push_back(std::move(selection));
      }
    }
    for (const auto &predicate : action.effects) {
      auto selection = get_referenced_parameters(action, predicate);
      if (std::find(selections_[i].begin(), selections_[i].end(), selection) ==
          selections_[i].end()) {
        selections_[i].push_back(std::move(selection));
      }
    }
    std::sort(selections_[i].begin(), selections_[i].end(),
              [](const auto &first, const auto &second) {
                return first.size() < second.size();
              });
    for (auto selection = selections_[i].begin();
         selection != selections_[i].end(); ++selection) {
      assignments_[i].emplace_back();
      for (auto assignment = AssignmentIterator{*selection, action, *problem_};
           assignment != AssignmentIterator{}; ++assignment) {
        assignments_[i].back()[get_assignment_id(*assignment, *problem_)] =
            num_vars_++;
      }
    }
  }
  predicates_.reserve(support_.get_num_instantiations());
  for (size_t i = 0; i < support_.get_num_instantiations(); ++i) {
    if (support_.is_rigid(Support::PredicateId{i}, true)) {
      predicates_.push_back(SAT);
    } else if (support_.is_rigid(Support::PredicateId{i}, false)) {
      predicates_.push_back(UNSAT);
    } else {
      predicates_.push_back(num_vars_++);
    }
  }
  return true;
}

bool ForeachEncoder::encode_init() noexcept {
  for (size_t i = 0; i < support_.get_num_instantiations(); ++i) {
    auto literal = Literal{Variable{predicates_[i]},
                           support_.is_init(Support::PredicateId{i})};
    init_ << literal << sat::EndClause;
  }
  return true;
}

bool ForeachEncoder::encode_actions() noexcept {
  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    const auto &action = problem_->actions[i];
    auto action_var = Variable{actions_[i]};
    for (size_t j = 0; j < selections_[i].size(); ++j) {
      universal_clauses_ << Literal{action_var, false};
      std::vector<Variable> all_assignments;
      all_assignments.reserve(assignments_[i][j].size());
      for (auto [id, var] : assignments_[i][j]) {
        universal_clauses_ << Literal{Variable{var}, true};
        all_assignments.push_back(Variable{var});
      }
      universal_clauses_ << sat::EndClause;
      for (auto var : all_assignments) {
        universal_clauses_ << Literal{var, false} << Literal{action_var, true}
                           << sat::EndClause;
      }
      universal_clauses_.at_most_one(all_assignments);
      const auto &first = selections_[i][j];
      for (size_t k = j + 1; k < selections_[i].size(); ++k) {
        if (config.check_timeout()) {
          return false;
        }
        const auto &second = selections_[i][k];
        ParameterSelection only_first;
        ParameterSelection both;
        ParameterSelection only_second;
        std::set_difference(first.begin(), first.end(), second.begin(),
                            second.end(), std::back_inserter(only_first));
        std::set_difference(second.begin(), second.end(), first.begin(),
                            first.end(), std::back_inserter(only_second));
        std::set_intersection(first.begin(), first.end(), second.begin(),
                              second.end(), std::back_inserter(both));
        for (auto both_it = AssignmentIterator{both, action, *problem_};
             both_it != AssignmentIterator{}; ++both_it) {
          std::vector<Variable> second_assignments;
          second_assignments.reserve(
              get_num_instantiated(only_second, action, *problem_));
          for (auto second_it =
                   AssignmentIterator{only_second, action, *problem_};
               second_it != AssignmentIterator{}; ++second_it) {
            ParameterAssignment second_assignment;
            std::merge((*second_it).begin(), (*second_it).end(),
                       (*both_it).begin(), (*both_it).end(),
                       std::back_inserter(second_assignment));
            second_assignments.push_back(Variable{
                assignments_[i][k]
                            [get_assignment_id(second_assignment, *problem_)]});
          }
          for (auto first_it =
                   AssignmentIterator{only_first, action, *problem_};
               first_it != AssignmentIterator{}; ++first_it) {
            ParameterAssignment first_assignment;
            std::merge((*first_it).begin(), (*first_it).end(),
                       (*both_it).begin(), (*both_it).end(),
                       std::back_inserter(first_assignment));
            universal_clauses_
                << Literal{Variable{assignments_[i][j][get_assignment_id(
                               first_assignment, *problem_)]},
                           false};
            for (auto v : second_assignments) {
              universal_clauses_ << Literal{v, true};
            }
            universal_clauses_ << sat::EndClause;
          }
        }
      }
    }
  }
  return true;
}

bool ForeachEncoder::parameter_implies_predicate() noexcept {
  for (size_t i = 0; i < support_.get_num_instantiations(); ++i) {
    if (config.check_timeout()) {
      return false;
    }
    for (bool positive : {true, false}) {
      for (bool is_effect : {true, false}) {
        auto &formula = is_effect ? transition_clauses_ : universal_clauses_;
        for (const auto &[action_index, assignment] : support_.get_support(
                 Support::PredicateId{i}, positive, is_effect)) {
          if (assignment.empty()) {
            formula << Literal{Variable{actions_[action_index]}, false};
          } else {
            formula << Literal{
                Variable{assignments_[action_index][get_selection_index(
                    action_index, assignment)][get_assignment_id(assignment,
                                                                 *problem_)]},
                false};
          }
          formula << Literal{Variable{predicates_[i], !is_effect}, positive}
                  << sat::EndClause;
        }
      }
    }
  }
  return true;
}

bool ForeachEncoder::interference() noexcept {
  for (size_t i = 0; i < support_.get_num_instantiations(); ++i) {
    if (config.check_timeout()) {
      return false;
    }
    for (bool positive : {true, false}) {
      const auto &precondition_support =
          support_.get_support(Support::PredicateId{i}, positive, false);
      const auto &effect_support =
          support_.get_support(Support::PredicateId{i}, !positive, true);
      for (const auto &[p_action_index, p_assignment] : precondition_support) {
        for (const auto &[e_action_index, e_assignment] : effect_support) {
          if (p_action_index == e_action_index) {
            continue;
          }
          for (bool is_effect : {true, false}) {
            const auto &assignment = is_effect ? e_assignment : p_assignment;
            auto action_index = is_effect ? e_action_index : p_action_index;
            if (assignment.empty()) {
              universal_clauses_
                  << Literal{Variable{actions_[action_index]}, false};
            } else {
              universal_clauses_ << Literal{
                  Variable{assignments_[action_index][get_selection_index(
                      action_index, assignment)][get_assignment_id(assignment,
                                                                   *problem_)]},
                  false};
            }
          }
          universal_clauses_ << sat::EndClause;
        }
      }
    }
  }
  return true;
}

bool ForeachEncoder::frame_axioms() noexcept {
  for (size_t i = 0; i < support_.get_num_instantiations(); ++i) {
    for (bool positive : {true, false}) {
      transition_clauses_ << Literal{Variable{predicates_[i], true}, positive}
                          << Literal{Variable{predicates_[i], false},
                                     !positive};
      for (const auto &[action_index, assignment] :
           support_.get_support(Support::PredicateId{i}, positive, true)) {
        if (assignment.empty()) {
          transition_clauses_
              << Literal{Variable{actions_[action_index]}, true};
        } else {
          transition_clauses_ << Literal{
              Variable{assignments_[action_index][get_selection_index(
                  action_index, assignment)]
                                   [get_assignment_id(assignment, *problem_)]},
              true};
        }
      }
      transition_clauses_ << sat::EndClause;
    }
  }
  return true;
}

bool ForeachEncoder::assume_goal() noexcept {
  for (const auto &[goal, positive] : problem_->goal) {
    goal_ << Literal{Variable{predicates_[support_.get_id(goal)]}, positive}
          << sat::EndClause;
  }
  return true;
}
