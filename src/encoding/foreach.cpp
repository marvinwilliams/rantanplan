#include "encoding/foreach.hpp"
#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized_problem.hpp"
#include "model/support.hpp"
#include "planning/planner.hpp"

using namespace normalized;

logging::Logger ForeachEncoder::logger{"Foreach"};

ForeachEncoder::ForeachEncoder(const Problem &problem,
                               const Config &config) noexcept
    : support_{problem} {
  init_sat_vars();
  LOG_INFO(logger, "Encoding...");
  encode_init();
  encode_actions();
  parameter_implies_predicate();
  interference();
  frame_axioms(config.dnf_threshold);
  assume_goal();
  num_vars_ -= 3; // subtract SAT und UNSAT for correct step semantics
  LOG_INFO(logger, "Representation uses %u variables", num_vars_);
}

int ForeachEncoder::get_sat_var(Literal literal, unsigned int step) const {
  size_t variable = 0;
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
    for (size_t i = 0; i < support_.get_num_actions(); ++i) {
      if (model[actions_[i] + s * num_vars_]) {
        const Action &action = support_.get_problem().actions[i];
        std::vector<const Constant *> constants;
        for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
             ++parameter_pos) {
          auto &parameter = action.parameters[parameter_pos];
          if (parameter.is_constant()) {
            constants.push_back(&parameter.get_constant());
          } else {
            for (size_t j = 0;
                 j < support_.get_problem()
                         .constants_by_type[get_index(&parameter.get_type(),
                                                      support_.get_problem())]
                         .size();
                 ++j) {
              if (model[parameters_[i][parameter_pos][j] + s * num_vars_]) {
                constants.push_back(
                    support_.get_constants_of_type(parameter.get_type())[j]);
                break;
              }
            }
          }
          assert(constants.size() == parameter_pos + 1);
        }
        plan.emplace_back(ActionHandle{i}, std::move(constants));
      }
    }
  }
  return plan;
}

void ForeachEncoder::encode_init() noexcept {
  for (size_t i = 0; i < support_.get_num_instantiations(); ++i) {
    auto literal = Literal{PredicateVariable{InstantiationHandle{i}, true},
                           support_.is_init(InstantiationHandle{i})};
    init_ << literal << sat::EndClause;
  }
}

void ForeachEncoder::encode_actions() {
  for (size_t i = 0; i < support_.get_num_actions(); ++i) {
    const auto &action = support_.get_problem().actions[i];
    for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
         ++parameter_pos) {
      const auto &parameter = action.parameters[parameter_pos];
      if (parameter.is_constant()) {
        continue;
      }
      size_t number_arguments =
          support_.get_constants_of_type(parameter.get_type()).size();
      std::vector<Variable> all_arguments;
      all_arguments.reserve(number_arguments);
      auto action_var = ActionVariable{ActionHandle{i}};
      for (size_t constant_index = 0; constant_index < number_arguments;
           ++constant_index) {
        auto parameter_var =
            ParameterVariable{ActionHandle{i}, parameter_pos, constant_index};
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

void ForeachEncoder::parameter_implies_predicate() {
  for (size_t i = 0; i < support_.get_num_instantiations(); ++i) {
    for (bool positive : {true, false}) {
      for (bool is_effect : {true, false}) {
        auto &formula = is_effect ? transition_clauses_ : universal_clauses_;
        for (const auto &[action_handle, assignment] : support_.get_support(
                 InstantiationHandle{i}, positive, is_effect)) {
          if (assignment.empty()) {
            formula << Literal{ActionVariable{action_handle}, false};
          } else {
            for (auto [parameter_index, constant_handle] : assignment) {
              const auto &constants = support_.get_constants_of_type(
                  support_.get_problem()
                      .actions[action_handle]
                      .parameters[parameter_index]
                      .get_type());
              auto it = std::find(constants.begin(), constants.end(),
                                  constant_handle);
              assert(it != constants.end());
              formula << Literal{
                  ParameterVariable{action_handle, parameter_index,
                                    static_cast<size_t>(
                                        std::distance(constants.begin(), it))},
                  false};
            }
          }
          formula << Literal{PredicateVariable{InstantiationHandle{i},
                                               !is_effect},
                             positive}
                  << sat::EndClause;
        }
      }
    }
  }
}

void ForeachEncoder::interference() {
  for (size_t i = 0; i < support_.get_num_instantiations(); ++i) {
    for (bool positive : {true, false}) {
      const auto &precondition_support =
          support_.get_support(InstantiationHandle{i}, positive, false);
      const auto &effect_support =
          support_.get_support(InstantiationHandle{i}, !positive, true);
      for (const auto &[p_handle, p_assignment] : precondition_support) {
        for (const auto &[e_handle, e_assignment] : effect_support) {
          if (p_handle == e_handle) {
            continue;
          }
          for (bool is_effect : {true, false}) {
            const auto &assignment = is_effect ? e_assignment : p_assignment;
            auto handle = is_effect ? e_handle : p_handle;
            if (assignment.empty()) {
              universal_clauses_ << Literal{ActionVariable{handle}, false};
            } else {
              for (auto [parameter_index, constant_handle] : assignment) {
                const auto &constants = support_.get_constants_of_type(
                    support_.get_problem()
                        .actions[handle]
                        .parameters[parameter_index]
                        .get_type());
                auto it = std::find(constants.begin(), constants.end(),
                                    constant_handle);
                assert(it != constants.end());
                universal_clauses_ << Literal{
                    ParameterVariable{handle, parameter_index,
                                      static_cast<size_t>(std::distance(
                                          constants.begin(), it))},
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

void ForeachEncoder::frame_axioms(size_t dnf_threshold) {
  for (size_t i = 0; i < support_.get_num_instantiations(); ++i) {
    for (bool positive : {true, false}) {
      size_t num_nontrivial_clauses = 0;
      for (const auto &[action_handle, assignment] :
           support_.get_support(InstantiationHandle{i}, positive, true)) {
        // Assignments with multiple arguments lead to combinatorial explosion
        if (assignment.size() > 1) {
          ++num_nontrivial_clauses;
        }
      }

      Formula dnf;
      dnf << Literal{PredicateVariable{InstantiationHandle{i}, true}, positive}
          << sat::EndClause;
      dnf << Literal{PredicateVariable{InstantiationHandle{i}, false},
                     !positive}
          << sat::EndClause;
      for (const auto &[action_handle, assignment] :
           support_.get_support(InstantiationHandle{i}, positive, true)) {
        if (assignment.empty()) {
          dnf << Literal{ActionVariable{action_handle}, true};
        } else if (assignment.size() == 1 ||
                   num_nontrivial_clauses < dnf_threshold) {
          for (auto [parameter_index, constant_handle] : assignment) {
            const auto &constants =
                support_.get_constants_of_type(support_.get_problem()
                                                   .actions[action_handle]
                                                   .parameters[parameter_index]
                                                   .get_type());
            auto it =
                std::find(constants.begin(), constants.end(), constant_handle);
            assert(it != constants.end());
            dnf << Literal{ParameterVariable{action_handle, parameter_index,
                                             static_cast<size_t>(std::distance(
                                                 constants.begin(), it))},
                           true};
          }
        } else {
          for (auto [parameter_index, constant_handle] : assignment) {
            universal_clauses_ << Literal{HelperVariable{num_helpers_}, false};
            const auto &constants =
                support_.get_constants_of_type(support_.get_problem()
                                                   .actions[action_handle]
                                                   .parameters[parameter_index]
                                                   .get_type());
            auto it =
                std::find(constants.begin(), constants.end(), constant_handle);
            assert(it != constants.end());
            universal_clauses_ << Literal{
                ParameterVariable{
                    action_handle, parameter_index,
                    static_cast<size_t>(std::distance(constants.begin(), it))},
                true};
            universal_clauses_ << sat::EndClause;
          }
          dnf << Literal{HelperVariable{num_helpers_}, true};
          ++num_vars_;
        }
        dnf << sat::EndClause;
      }
      transition_clauses_.add_dnf(dnf);
    }
  }
}

void ForeachEncoder::assume_goal() {
  for (const auto &[goal, positive] : support_.get_problem().goal) {
    goal_ << Literal{PredicateVariable{support_.get_predicate_handle(goal),
                                       true},
                     positive}
          << sat::EndClause;
  }
}

void ForeachEncoder::init_sat_vars() {
  LOG_INFO(logger, "Initializing sat variables...");
  actions_.reserve(support_.get_problem().actions.size());
  parameters_.resize(support_.get_problem().actions.size());
  for (size_t i = 0; i < support_.get_problem().actions.size(); ++i) {
    const auto &action = support_.get_problem().actions[i];
    actions_.push_back(num_vars_++);

    parameters_[i].resize(action.parameters.size());

    for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
         ++parameter_pos) {
      const auto &parameter = action.parameters[parameter_pos];
      if (parameter.is_constant()) {
        continue;
      }
      parameters_[i][parameter_pos].reserve(
          support_.get_constants_of_type(parameter.get_type()).size());
      for (size_t j = 0;
           j < support_.get_constants_of_type(parameter.get_type()).size();
           ++j) {
        parameters_[i][parameter_pos].push_back(num_vars_++);
      }
    }
  }

  predicates_.resize(support_.get_num_instantiations());
  for (size_t i = 0; i < support_.get_num_instantiations(); ++i) {
    if (support_.is_rigid(InstantiationHandle{i}, true)) {
      predicates_[i] = SAT;
    } else if (support_.is_rigid(InstantiationHandle{i}, false)) {
      predicates_[i] = UNSAT;
    } else {
      predicates_[i] = num_vars_++;
    }
  }
}
