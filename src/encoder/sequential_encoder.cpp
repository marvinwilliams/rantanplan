#include "encoder/sequential_encoder.hpp"
#include "config.hpp"
#include "encoder/encoder.hpp"
#include "encoder/support.hpp"
#include "grounder/grounder.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "planner/planner.hpp"
#include "sat/formula.hpp"
#include "util/timer.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <vector>

using namespace normalized;

SequentialEncoder::SequentialEncoder(const std::shared_ptr<Problem> &problem)
    : Encoder{problem}, support_{*problem} {
  LOG_INFO(encoding_logger, "Init sat variables...");
  init_sat_vars();
  LOG_INFO(encoding_logger, "Encode problem...");
  encode_init();
  encode_actions();
  parameter_implies_predicate();
  frame_axioms();
  assume_goal();
  num_vars_ -= 3; // subtract SAT und UNSAT for correct step semantics

  LOG_INFO(encoding_logger, "Variables per step: %lu", num_vars_);
  LOG_INFO(encoding_logger, "Helper variables to mitigate dnf explosion: %lu",
           std::accumulate(
               dnf_helpers_.begin(), dnf_helpers_.end(), 0ul,
               [](size_t sum, const auto &m) { return sum + m.size(); }));
  LOG_INFO(encoding_logger, "Init clauses: %lu", init_.clauses.size());
  LOG_INFO(encoding_logger, "Universal clauses: %lu",
           universal_clauses_.clauses.size());
  LOG_INFO(encoding_logger, "Transition clauses: %lu",
           transition_clauses_.clauses.size());
  LOG_INFO(encoding_logger, "Goal clauses: %lu", goal_.clauses.size());
}

int SequentialEncoder::to_sat_var(Literal l, unsigned int step) const noexcept {
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

Plan SequentialEncoder::extract_plan(const sat::Model &model,
                                     unsigned int step) const noexcept {
  Plan plan;
  plan.problem = problem_;
  for (unsigned int s = 0; s < step; ++s) {
    for (size_t i = 0; i < problem_->actions.size(); ++i) {
      if (model[actions_[i] + s * num_vars_]) {
        const Action &action = problem_->actions[i];
        std::vector<Constant> constants;
        for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
             ++parameter_pos) {
          const auto &parameter = action.parameters[parameter_pos];
          if (!parameter.is_free()) {
            constants.push_back(parameter.get_constant());
            continue;
          }
          for (size_t j = 0; j < problem_->constants.size(); ++j) {
            if (model[parameters_[parameter_pos][j] + s * num_vars_]) {
              constants.push_back(problem_->constants[j]);
              break;
            }
          }
          assert(constants.size() == parameter_pos + 1);
        }
        plan.sequence.emplace_back(ActionIndex{i}, std::move(constants));
        break;
      }
    }
  }
  return plan;
}

void SequentialEncoder::init_sat_vars() {
  actions_.reserve(problem_->actions.size());
  auto last_parameter = [](const auto &action) {
    return static_cast<size_t>(std::distance(
        action.parameters.begin(),
        std::find_if(action.parameters.rbegin(), action.parameters.rend(),
                     [](const Parameter &p) { return p.is_free(); })
            .base()));
  };
  parameters_.resize(last_parameter(
      *std::max_element(problem_->actions.begin(), problem_->actions.end(),
                        [&](const auto &a1, const auto &a2) {
                          return last_parameter(a1) < last_parameter(a2);
                        })));
  dnf_helpers_.resize(problem_->actions.size());
  for (size_t i = 0; i < parameters_.size(); ++i) {
    parameters_[i].reserve(problem_->constants.size());
    for (size_t j = 0; j < problem_->constants.size(); ++j) {
      parameters_[i].push_back(num_vars_++);
    }
  }
  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    actions_.push_back(num_vars_++);
  }

  predicates_.resize(support_.get_num_ground_atoms());
  for (size_t i = 0; i < support_.get_num_ground_atoms(); ++i) {
    if (support_.is_rigid(Support::PredicateId{i}, true)) {
      predicates_[i] = SAT;
    } else if (support_.is_rigid(Support::PredicateId{i}, false)) {
      predicates_[i] = UNSAT;
    } else {
      predicates_[i] = num_vars_++;
    }
  }
}

void SequentialEncoder::encode_init() {
  for (size_t i = 0; i < support_.get_num_ground_atoms(); ++i) {
    auto literal = Literal{Variable{predicates_[i]},
                           support_.is_init(Support::PredicateId{i})};
    init_ << literal << sat::EndClause;
  }
}

void SequentialEncoder::encode_actions() {
  uint_fast64_t clause_count = 0;
  for (size_t i = 0; i < parameters_.size(); ++i) {
    std::vector<Variable> all_arguments;
    all_arguments.reserve(problem_->constants.size());
    for (size_t j = 0; j < problem_->constants.size(); ++j) {
      all_arguments.push_back(Variable{parameters_[i][j]});
    }
    clause_count += universal_clauses_.at_most_one(all_arguments);
  }
  std::vector<Variable> all_actions;
  all_actions.reserve(problem_->actions.size());
  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    const auto &action = problem_->actions[i];
    auto action_var = Variable{actions_[i]};
    for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
         ++parameter_pos) {
      const auto &parameter = action.parameters[parameter_pos];
      if (!parameter.is_free()) {
        continue;
      }
      const auto &constants_by_type =
          problem_->constants_of_type[parameter.get_type().id];
      universal_clauses_ << Literal{action_var, false};
      for (size_t constant_index = 0; constant_index < constants_by_type.size();
           ++constant_index) {
        universal_clauses_ << Literal{
            Variable{parameters_[parameter_pos]
                                [constants_by_type[constant_index].id]},
            true};
      }
      universal_clauses_ << sat::EndClause;
      ++clause_count;
    }
    all_actions.push_back(action_var);
  }
  clause_count += universal_clauses_.at_most_one(all_actions);
  LOG_INFO(encoding_logger, "Action clauses: %lu", clause_count);
}

void SequentialEncoder::parameter_implies_predicate() {
  uint_fast64_t clause_count = 0;
  for (size_t i = 0; i < support_.get_num_ground_atoms(); ++i) {
    if (global_timer.get_elapsed_time() > config.timeout) {
      throw TimeoutException{};
    }
    for (bool positive : {true, false}) {
      for (bool is_effect : {true, false}) {
        auto &formula = is_effect ? transition_clauses_ : universal_clauses_;
        for (const auto &[action_index, assignment] : support_.get_support(
                 Support::PredicateId{i}, positive, is_effect)) {
          formula << Literal{Variable{actions_[action_index]}, false};
          for (auto [parameter_index, constant] : assignment) {
            formula << Literal{
                Variable{parameters_[parameter_index][constant.id]}, false};
          }
          formula << Literal{Variable{predicates_[i], !is_effect}, positive}
                  << sat::EndClause;
          ++clause_count;
        }
      }
    }
  }
  LOG_INFO(encoding_logger, "Implication clauses: %lu", clause_count);
}

void SequentialEncoder::frame_axioms() {
  uint_fast64_t clause_count = 0;
  for (size_t i = 0; i < support_.get_num_ground_atoms(); ++i) {
    if (global_timer.get_elapsed_time() > config.timeout) {
      throw TimeoutException{};
    }
    for (bool positive : {true, false}) {
      bool use_helper = false;
      if (config.dnf_threshold > 0) {
        size_t num_nontrivial_clauses = 0;
        for (const auto &[action_index, assignment] :
             support_.get_support(Support::PredicateId{i}, positive, true)) {
          // Assignments with multiple arguments lead to combinatorial explosion
          if (!assignment.empty()) {
            ++num_nontrivial_clauses;
          }
        }
        use_helper = num_nontrivial_clauses >= config.dnf_threshold;
      }
      Formula dnf;
      dnf << Literal{Variable{predicates_[i], true}, positive}
          << sat::EndClause;
      dnf << Literal{Variable{predicates_[i], false}, !positive}
          << sat::EndClause;
      for (const auto &[action_index, assignment] :
           support_.get_support(Support::PredicateId{i}, positive, true)) {
        if (use_helper && !assignment.empty()) {
          auto [it, success] =
              dnf_helpers_[action_index].try_emplace(assignment, num_vars_);
          if (success) {
            universal_clauses_ << Literal{Variable{it->second}, false};
            universal_clauses_
                << Literal{Variable{actions_[action_index]}, true};
            universal_clauses_ << sat::EndClause;
            ++clause_count;
            for (auto [parameter_index, constant] : assignment) {
              universal_clauses_ << Literal{Variable{it->second}, false};
              universal_clauses_ << Literal{
                  Variable{parameters_[parameter_index][constant.id]}, true};
              universal_clauses_ << sat::EndClause;
            }
            ++num_vars_;
            clause_count += assignment.size();
          }
          dnf << Literal{Variable{it->second}, true};
        } else {
          dnf << Literal{Variable{actions_[action_index]}, true};
          for (auto [parameter_index, constant] : assignment) {
            dnf << Literal{Variable{parameters_[parameter_index][constant.id]},
                           true};
          }
        }
        dnf << sat::EndClause;
      }
      clause_count += transition_clauses_.add_dnf(dnf);
    }
  }
  LOG_INFO(encoding_logger, "Frame axiom clauses: %lu", clause_count);
}

void SequentialEncoder::assume_goal() {
  for (const auto &[goal, positive] : problem_->goal) {
    goal_ << Literal{Variable{predicates_[support_.get_id(goal)]}, positive}
          << sat::EndClause;
  }
}
