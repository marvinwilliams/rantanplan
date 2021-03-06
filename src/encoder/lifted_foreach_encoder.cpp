#include "encoder/lifted_foreach_encoder.hpp"
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

LiftedForeachEncoder::LiftedForeachEncoder(
    const std::shared_ptr<Problem> &problem,
    util::Seconds timeout = util::inf_time)
    : Encoder{problem, timeout}, support_{*problem, timeout} {
  LOG_INFO(encoding_logger, "Init sat variables...");
  init_sat_vars();
}

void LiftedForeachEncoder::encode() {
  LOG_INFO(encoding_logger, "Encode problem...");
  encode_init();
  encode_actions();
  parameter_implies_predicate();
  interference();
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

int LiftedForeachEncoder::to_sat_var(Literal l, unsigned int step) const
    noexcept {
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

Plan LiftedForeachEncoder::extract_plan(const sat::Model &model,
                                        unsigned int step) const noexcept {
  Plan plan;
  plan.problem = problem_;
  for (unsigned int s = 0; s < step; ++s) {
    for (size_t i = 0; i < problem_->actions.size(); ++i) {
      if (model[actions_[i] + s * num_vars_]) {
        const Action &action = problem_->actions[i];
        std::vector<ConstantIndex> constants;
        for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
             ++parameter_pos) {
          auto &parameter = action.parameters[parameter_pos];
          if (!parameter.is_free()) {
            constants.push_back(parameter.get_constant());
          } else {
            for (size_t j = 0;
                 j < problem_->constants_of_type[parameter.get_type()].size();
                 ++j) {
              if (model[parameters_[i][parameter_pos][j] + s * num_vars_]) {
                constants.push_back(
                    problem_->constants_of_type[parameter.get_type()][j]);
                break;
              }
            }
          }
          assert(constants.size() == parameter_pos + 1);
        }
        plan.sequence.emplace_back(ActionIndex{i}, std::move(constants));
      }
    }
  }
  return plan;
}

void LiftedForeachEncoder::init_sat_vars() {
  actions_.reserve(problem_->actions.size());
  parameters_.resize(problem_->actions.size());
  dnf_helpers_.resize(problem_->actions.size());
  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    const auto &action = problem_->actions[i];
    actions_.push_back(num_vars_++);

    parameters_[i].resize(action.parameters.size());

    for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
         ++parameter_pos) {
      const auto &parameter = action.parameters[parameter_pos];
      if (!parameter.is_free()) {
        continue;
      }
      parameters_[i][parameter_pos].reserve(
          problem_->constants_of_type[parameter.get_type()].size());
      for (size_t j = 0;
           j < problem_->constants_of_type[parameter.get_type()].size(); ++j) {
        parameters_[i][parameter_pos].push_back(num_vars_++);
      }
    }
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

size_t LiftedForeachEncoder::get_constant_index(ConstantIndex constant,
                                                TypeIndex type) const noexcept {
  auto it = problem_->constant_type_map[type].find(constant);
  if (it != problem_->constant_type_map[type].end()) {
    return it->second;
  }
  assert(false);
  return problem_->constants.size();
}

void LiftedForeachEncoder::encode_init() {
  for (size_t i = 0; i < support_.get_num_ground_atoms(); ++i) {
    auto literal = Literal{Variable{predicates_[i]},
                           support_.is_init(Support::PredicateId{i})};
    init_ << literal << sat::EndClause;
  }
}

void LiftedForeachEncoder::encode_actions() {
  uint_fast64_t clause_count = 0;
  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    const auto &action = problem_->actions[i];
    auto action_var = Variable{actions_[i]};
    for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
         ++parameter_pos) {
      const auto &parameter = action.parameters[parameter_pos];
      if (!parameter.is_free()) {
        continue;
      }
      size_t number_arguments =
          problem_->constants_of_type[parameter.get_type()].size();
      std::vector<Variable> all_arguments;
      all_arguments.reserve(number_arguments);
      universal_clauses_ << Literal{action_var, false};
      for (size_t constant_index = 0; constant_index < number_arguments;
           ++constant_index) {
        auto argument = Variable{parameters_[i][parameter_pos][constant_index]};
        universal_clauses_ << Literal{argument, true};
        all_arguments.push_back(argument);
      }
      universal_clauses_ << sat::EndClause;
      ++clause_count;
      clause_count += universal_clauses_.at_most_one(all_arguments);
      if (config.parameter_implies_action) {
        for (auto argument : all_arguments) {
          universal_clauses_ << Literal{argument, false};
          universal_clauses_ << Literal{action_var, true};
          universal_clauses_ << sat::EndClause;
        }
        clause_count += all_arguments.size();
      }
    }
  }
  LOG_INFO(encoding_logger, "Action clauses: %lu", clause_count);
}

void LiftedForeachEncoder::parameter_implies_predicate() {
  uint_fast64_t clause_count = 0;
  for (size_t i = 0; i < support_.get_num_ground_atoms(); ++i) {
    if (check_timeout()) {
      throw TimeoutException{};
    }
    for (bool positive : {true, false}) {
      for (bool is_effect : {true, false}) {
        auto &formula = is_effect ? transition_clauses_ : universal_clauses_;
        for (const auto &[action_index, assignment] : support_.get_support(
                 Support::PredicateId{i}, positive, is_effect)) {
          if (!config.parameter_implies_action || assignment.empty()) {
            formula << Literal{Variable{actions_[action_index]}, false};
          }
          for (const auto &[parameter_index, constant] : assignment) {
            auto index =
                get_constant_index(constant, problem_->actions[action_index]
                                                 .parameters[parameter_index]
                                                 .get_type());
            formula << Literal{
                Variable{parameters_[action_index][parameter_index][index]},
                false};
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

void LiftedForeachEncoder::interference() {
  auto has_disabling_effect = [this](const auto &first_action,
                                     const auto &second_action) {
    for (const auto &precondition : first_action.preconditions) {
      for (const auto &effect : second_action.effects) {
        if (precondition.atom.predicate == effect.atom.predicate &&
            precondition.positive != effect.positive &&
            is_unifiable(precondition.atom, first_action, effect.atom,
                         second_action, *problem_)) {
          return true;
        }
      }
      for (const auto &[effect, positive] : second_action.ground_effects) {
        if (precondition.atom.predicate == effect.predicate &&
            precondition.positive != positive &&
            is_instantiatable(precondition.atom, effect.arguments, first_action,
                              *problem_)) {
          return true;
        }
      }
    }
    for (const auto &[precondition, positive] :
         first_action.ground_preconditions) {
      for (const auto &effect : second_action.effects) {
        if (precondition.predicate == effect.atom.predicate &&
            positive != effect.positive &&
            is_instantiatable(effect.atom, precondition.arguments,
                              second_action, *problem_)) {
          return true;
        }
      }
      for (const auto &[effect, eff_positive] : second_action.ground_effects) {
        if (precondition.predicate == effect.predicate &&
            positive != eff_positive && precondition.arguments == effect.arguments) {
          return true;
        }
      }
    }
    return false;
  };

  uint_fast64_t clause_count = 0;
  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    for (size_t j = 0; j < problem_->actions.size(); ++j) {
      if (i == j) {
        continue;
      }
      if (check_timeout()) {
        throw TimeoutException{};
      }
      if (has_disabling_effect(problem_->actions[i], problem_->actions[j])) {
        universal_clauses_ << Literal{Variable{actions_[i]}, false}
                           << Literal{Variable{actions_[j]}, false};
        universal_clauses_ << sat::EndClause;
        ++clause_count;
      }
    }
  }
  LOG_INFO(encoding_logger, "Interference clauses: %lu", clause_count);
}

void LiftedForeachEncoder::frame_axioms() {
  uint_fast64_t clause_count = 0;
  for (size_t i = 0; i < support_.get_num_ground_atoms(); ++i) {
    if (check_timeout()) {
      throw TimeoutException{};
    }
    for (bool positive : {true, false}) {
      bool use_helper = false;
      if (config.dnf_threshold > 0) {
        size_t num_nontrivial_clauses = 0;
        for (const auto &[action_index, assignment] :
             support_.get_support(Support::PredicateId{i}, positive, true)) {
          // Assignments with multiple arguments lead to combinatorial explosion
          if (assignment.size() > (config.parameter_implies_action ? 1 : 0)) {
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
        if (use_helper &&
            assignment.size() > (config.parameter_implies_action ? 1 : 0)) {
          auto [it, success] =
              dnf_helpers_[action_index].try_emplace(assignment, num_vars_);
          if (success) {
            if (!config.parameter_implies_action) {
              universal_clauses_ << Literal{Variable{it->second}, false};
              universal_clauses_
                  << Literal{Variable{actions_[action_index]}, true};
              universal_clauses_ << sat::EndClause;
              ++clause_count;
            }
            for (const auto &[parameter_index, constant] : assignment) {
              auto index =
                  get_constant_index(constant, problem_->actions[action_index]
                                                   .parameters[parameter_index]
                                                   .get_type());
              universal_clauses_ << Literal{Variable{it->second}, false};
              universal_clauses_ << Literal{
                  Variable{parameters_[action_index][parameter_index][index]},
                  true};
              universal_clauses_ << sat::EndClause;
            }
            ++num_vars_;
            clause_count += assignment.size();
          }
          dnf << Literal{Variable{it->second}, true};
        } else {
          if (!config.parameter_implies_action || assignment.empty()) {
            dnf << Literal{Variable{actions_[action_index]}, true};
          }
          for (const auto &[parameter_index, constant] : assignment) {
            auto index =
                get_constant_index(constant, problem_->actions[action_index]
                                                 .parameters[parameter_index]
                                                 .get_type());
            dnf << Literal{
                Variable{parameters_[action_index][parameter_index][index]},
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

void LiftedForeachEncoder::assume_goal() {
  for (const auto &[goal, positive] : problem_->goal) {
    goal_ << Literal{Variable{predicates_[support_.get_id(goal)]}, positive}
          << sat::EndClause;
  }
}
