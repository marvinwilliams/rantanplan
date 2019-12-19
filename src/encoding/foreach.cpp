#include "encoding/foreach.hpp"
#include "config.hpp"
#include "encoding/support.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "planning/planner.hpp"

#include <algorithm>
#include <iterator>
#include <vector>

using namespace normalized;

logging::Logger ForeachEncoder::logger{"Foreach"};

ForeachEncoder::ForeachEncoder(const Problem &problem,
                               const Config &config) noexcept
    : problem_{problem}, support_{problem_} {
  LOG_INFO(logger, "Encoding...");
  init_sat_vars();
  encode_init();
  encode_actions();
  parameter_implies_predicate();
  interference();
  frame_axioms(config.dnf_threshold);
  assume_goal();
  num_vars_ -= 3; // subtract SAT und UNSAT for correct step semantics
  LOG_INFO(logger, "Representation uses %u variables per step", num_vars_);
}

int ForeachEncoder::get_sat_var(Literal literal, unsigned int step) const {
  uint_fast64_t variable = 0;
  variable = literal.variable.sat_var;
  if (variable == DONTCARE) {
    return static_cast<int>(SAT);
  }
  if (variable == SAT || variable == UNSAT) {
    return (literal.positive ? 1 : -1) * static_cast<int>(variable);
  }
  step += literal.variable.this_step ? 0 : 1;
  return (literal.positive ? 1 : -1) *
         static_cast<int>(variable + step * num_vars_);
}

Planner::Plan ForeachEncoder::extract_plan(const sat::Model &model,
                                           unsigned int step) const noexcept {
  Planner::Plan plan;
  for (unsigned int s = 0; s < step; ++s) {
    for (size_t i = 0; i < problem_.actions.size(); ++i) {
      if (model[actions_[i] + s * num_vars_]) {
        const Action &action = problem_.actions[i];
        std::vector<ConstantIndex> constants;
        for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
             ++parameter_pos) {
          auto &parameter = action.parameters[parameter_pos];
          if (parameter.is_constant()) {
            constants.push_back(parameter.get_constant());
          } else {
            for (size_t j = 0;
                 j < problem_.constants_by_type[parameter.get_type()].size();
                 ++j) {
              if (model[parameters_[i][parameter_pos][j] + s * num_vars_]) {
                constants.push_back(
                    problem_.constants_by_type[parameter.get_type()][j]);
                break;
              }
            }
          }
          assert(constants.size() == parameter_pos + 1);
        }
        plan.emplace_back(ActionIndex{i}, std::move(constants));
      }
    }
  }
  return plan;
}

void ForeachEncoder::encode_init() noexcept {
  for (size_t i = 0; i < support_.get_num_instantiations(); ++i) {
    auto literal = Literal{Variable{predicates_[i]},
                           support_.is_init(Support::PredicateId{i})};
    init_ << literal << sat::EndClause;
  }
}

void ForeachEncoder::encode_actions() noexcept {
  for (size_t i = 0; i < problem_.actions.size(); ++i) {
    const auto &action = problem_.actions[i];
    for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
         ++parameter_pos) {
      const auto &parameter = action.parameters[parameter_pos];
      if (parameter.is_constant()) {
        continue;
      }
      size_t number_arguments =
          problem_.constants_by_type[parameter.get_type()].size();
      std::vector<Variable> all_arguments;
      all_arguments.reserve(number_arguments);
      auto action_var = Variable{actions_[i]};
      for (size_t constant_index = 0; constant_index < number_arguments;
           ++constant_index) {
        auto parameter_var =
            Variable{parameters_[i][parameter_pos][constant_index]};
        universal_clauses_ << Literal{parameter_var, false};
        universal_clauses_ << Literal{action_var, true};
        universal_clauses_ << sat::EndClause;
        all_arguments.push_back(parameter_var);
      }
      universal_clauses_ << Literal{action_var, false};
      for (const auto &argument : all_arguments) {
        universal_clauses_ << Literal{argument, true};
      }
      universal_clauses_ << sat::EndClause;
      universal_clauses_.at_most_one(all_arguments);
    }
  }
}

void ForeachEncoder::parameter_implies_predicate() noexcept {
  for (size_t i = 0; i < support_.get_num_instantiations(); ++i) {
    for (bool positive : {true, false}) {
      for (bool is_effect : {true, false}) {
        auto &formula = is_effect ? transition_clauses_ : universal_clauses_;
        for (const auto &[action_index, assignment] : support_.get_support(
                 Support::PredicateId{i}, positive, is_effect)) {
          if (assignment.empty()) {
            formula << Literal{Variable{actions_[action_index]}, false};
          } else {
            for (auto [parameter_index, constant_index] : assignment) {
              const auto &constants =
                  problem_.constants_by_type[problem_.actions[action_index]
                                                 .parameters[parameter_index]
                                                 .get_type()];
              auto it =
                  std::find(constants.begin(), constants.end(), constant_index);
              assert(it != constants.end());
              formula << Literal{
                  Variable{
                      parameters_[action_index][parameter_index]
                                 [static_cast<size_t>(it - constants.begin())]},
                  false};
            }
          }
          formula << Literal{Variable{predicates_[i], !is_effect}, positive}
                  << sat::EndClause;
        }
      }
    }
  }
}

void ForeachEncoder::interference() noexcept {
  for (size_t i = 0; i < support_.get_num_instantiations(); ++i) {
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
              for (auto [parameter_index, constant_index] : assignment) {
                const auto &constants =
                    problem_.constants_by_type[problem_.actions[action_index]
                                                   .parameters[parameter_index]
                                                   .get_type()];
                auto it = std::find(constants.begin(), constants.end(),
                                    constant_index);
                assert(it != constants.end());
                universal_clauses_ << Literal{
                    Variable{parameters_[action_index][parameter_index]
                                        [static_cast<size_t>(
                                            it - constants.begin())]},
                    false};
              }
            }
          }
          universal_clauses_ << sat::EndClause;
        }
      }
    }
  }
}

void ForeachEncoder::frame_axioms(unsigned int dnf_threshold) noexcept {
  for (size_t i = 0; i < support_.get_num_instantiations(); ++i) {
    for (bool positive : {true, false}) {
      size_t num_nontrivial_clauses = 0;
      for (const auto &[action_index, assignment] :
           support_.get_support(Support::PredicateId{i}, positive, true)) {
        // Assignments with multiple arguments lead to combinatorial explosion
        if (assignment.size() > 1) {
          ++num_nontrivial_clauses;
        }
      }

      Formula dnf;
      dnf << Literal{Variable{predicates_[i]}, positive} << sat::EndClause;
      dnf << Literal{Variable{predicates_[i], false}, !positive}
          << sat::EndClause;
      for (const auto &[action_index, assignment] :
           support_.get_support(Support::PredicateId{i}, positive, true)) {
        if (assignment.empty()) {
          dnf << Literal{Variable{actions_[action_index]}, true};
        } else if (assignment.size() == 1 ||
                   num_nontrivial_clauses < dnf_threshold) {
          for (auto [parameter_index, constant_index] : assignment) {
            const auto &constants =
                problem_.constants_by_type[problem_.actions[action_index]
                                               .parameters[parameter_index]
                                               .get_type()];
            auto it =
                std::find(constants.begin(), constants.end(), constant_index);
            assert(it != constants.end());
            dnf << Literal{
                Variable{
                    parameters_[action_index][parameter_index]
                               [static_cast<size_t>(it - constants.begin())]},
                true};
          }
        } else {
          for (auto [parameter_index, constant_index] : assignment) {
            universal_clauses_ << Literal{Variable{num_vars_}, false};
            const auto &constants =
                problem_.constants_by_type[problem_.actions[action_index]
                                               .parameters[parameter_index]
                                               .get_type()];
            auto it =
                std::find(constants.begin(), constants.end(), constant_index);
            assert(it != constants.end());
            universal_clauses_ << Literal{
                Variable{
                    parameters_[action_index][parameter_index]
                               [static_cast<size_t>(it - constants.begin())]},
                true};
            universal_clauses_ << sat::EndClause;
          }
          dnf << Literal{Variable{num_vars_}, true};
          ++num_vars_;
        }
        dnf << sat::EndClause;
      }
      transition_clauses_.add_dnf(dnf);
    }
  }
}

void ForeachEncoder::assume_goal() noexcept {
  for (const auto &[goal, positive] : problem_.goal) {
    goal_ << Literal{Variable{predicates_[support_.get_id(goal)]}, positive}
          << sat::EndClause;
  }
}

void ForeachEncoder::init_sat_vars() noexcept {
  LOG_INFO(logger, "Initializing sat variables...");
  actions_.reserve(problem_.actions.size());
  parameters_.resize(problem_.actions.size());
  for (size_t i = 0; i < problem_.actions.size(); ++i) {
    const auto &action = problem_.actions[i];
    actions_.push_back(num_vars_++);

    parameters_[i].resize(action.parameters.size());

    for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
         ++parameter_pos) {
      const auto &parameter = action.parameters[parameter_pos];
      if (parameter.is_constant()) {
        continue;
      }
      parameters_[i][parameter_pos].reserve(
          problem_.constants_by_type[parameter.get_type()].size());
      for (size_t j = 0;
           j < problem_.constants_by_type[parameter.get_type()].size(); ++j) {
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
}
