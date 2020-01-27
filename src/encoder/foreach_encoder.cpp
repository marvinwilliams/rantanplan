#include "encoder/foreach_encoder.hpp"
#include "config.hpp"
#include "encoder/encoder.hpp"
#include "encoder/support.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
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
      if (model[actions_[i] + s * num_vars_]) {
        const Action &action = problem_->actions[i];
        std::vector<ConstantIndex> constants;
        for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
             ++parameter_pos) {
          auto &parameter = action.parameters[parameter_pos];
          if (parameter.is_constant()) {
            constants.push_back(parameter.get_constant());
          } else {
            for (size_t j = 0;
                 j < problem_->constants_by_type[parameter.get_type()].size();
                 ++j) {
              if (model[parameters_[i][parameter_pos][j] + s * num_vars_]) {
                constants.push_back(
                    problem_->constants_by_type[parameter.get_type()][j]);
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

bool ForeachEncoder::init_sat_vars() noexcept {
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
      if (parameter.is_constant()) {
        continue;
      }
      parameters_[i][parameter_pos].reserve(
          problem_->constants_by_type[parameter.get_type()].size());
      for (size_t j = 0;
           j < problem_->constants_by_type[parameter.get_type()].size(); ++j) {
        parameters_[i][parameter_pos].push_back(num_vars_++);
      }
    }
  }

  predicates_.resize(support_.get_num_instantiations());
  for (size_t i = 0; i < support_.get_num_instantiations(); ++i) {
    if (support_.is_rigid(Support::PredicateId{i}, true)) {
      predicates_[i] = SAT;
    } else if (support_.is_rigid(Support::PredicateId{i}, false)) {
      predicates_[i] = UNSAT;
    } else {
      predicates_[i] = num_vars_++;
    }
  }
  return true;
}

size_t ForeachEncoder::get_constant_index(ConstantIndex constant,
                                          TypeIndex type) const noexcept {
  for (size_t i = 0; i < problem_->constants_by_type[type].size(); ++i) {
    if (problem_->constants_by_type[type][i] == constant) {
      return i;
    }
  }
  assert(false);
  return problem_->constants_by_type[type].size();
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
    for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
         ++parameter_pos) {
      const auto &parameter = action.parameters[parameter_pos];
      if (parameter.is_constant()) {
        continue;
      }
      size_t number_arguments =
          problem_->constants_by_type[parameter.get_type()].size();
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
      universal_clauses_.at_most_one(all_arguments);
      if (config.parameter_implies_action) {
        for (auto argument : all_arguments) {
          universal_clauses_ << Literal{argument, false};
          universal_clauses_ << Literal{action_var, true};
          universal_clauses_ << sat::EndClause;
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
          if (!config.parameter_implies_action || assignment.empty()) {
            formula << Literal{Variable{actions_[action_index]}, false};
          }
          for (auto [parameter_index, constant_index] : assignment) {
            auto index = get_constant_index(constant_index,
                                            problem_->actions[action_index]
                                                .parameters[parameter_index]
                                                .get_type());
            formula << Literal{
                Variable{parameters_[action_index][parameter_index][index]},
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
            if (!config.parameter_implies_action || assignment.empty()) {
              universal_clauses_
                  << Literal{Variable{actions_[action_index]}, false};
            }
            for (auto [parameter_index, constant_index] : assignment) {
              auto index = get_constant_index(constant_index,
                                              problem_->actions[action_index]
                                                  .parameters[parameter_index]
                                                  .get_type());
              universal_clauses_ << Literal{
                  Variable{parameters_[action_index][parameter_index][index]},
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
    if (config.check_timeout()) {
      return false;
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
            }
            for (auto [parameter_index, constant_index] : assignment) {
              auto index = get_constant_index(constant_index,
                                              problem_->actions[action_index]
                                                  .parameters[parameter_index]
                                                  .get_type());
              universal_clauses_ << Literal{Variable{it->second}, false};
              universal_clauses_ << Literal{
                  Variable{parameters_[action_index][parameter_index][index]},
                  true};
              universal_clauses_ << sat::EndClause;
            }
            ++num_vars_;
          }
          dnf << Literal{Variable{it->second}, true};
        } else {
          if (!config.parameter_implies_action || assignment.empty()) {
            dnf << Literal{Variable{actions_[action_index]}, true};
          }
          for (auto [parameter_index, constant_index] : assignment) {
            auto index = get_constant_index(constant_index,
                                            problem_->actions[action_index]
                                                .parameters[parameter_index]
                                                .get_type());
            dnf << Literal{
                Variable{parameters_[action_index][parameter_index][index]},
                true};
          }
        }
        dnf << sat::EndClause;
      }
      transition_clauses_.add_dnf(dnf);
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
