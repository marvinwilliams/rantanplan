#include "encoding/foreach.hpp"
#include "logging/logging.hpp"

namespace encoding {

logging::Logger ForeachEncoder::logger{"Foreach"};

ForeachEncoder::ForeachEncoder(const support::Support &support,
                               const Config &config) noexcept
    : support_{support} {
  if (config.log_encoding) {
    logger.add_appender(logging::default_appender);
  }
  num_helpers_ = 0;
  encode_initial_state();
  encode_actions();
  parameter_implies_predicate(false, false);
  parameter_implies_predicate(false, true);
  parameter_implies_predicate(true, false);
  parameter_implies_predicate(true, true);
  interference(false);
  interference(true);
  frame_axioms(false, config.dnf_threshold);
  frame_axioms(true, config.dnf_threshold);
  assume_goal();
  init_sat_vars();
}

int ForeachEncoder::get_sat_var(Literal literal, unsigned int step) const {
  size_t variable = 0;
  if (const ActionVariable *p = std::get_if<ActionVariable>(&literal.variable);
      p) {
    variable = actions_[p->action_handle];
  } else if (const PredicateVariable *p =
                 std::get_if<PredicateVariable>(&literal.variable);
             p) {
    variable = predicates_[p->predicate_handle][p->ground_predicate_handle];
    if (!p->this_step && variable > UNSAT) {
      variable += static_cast<size_t>(num_vars_);
    }
  } else if (const ParameterVariable *p =
                 std::get_if<ParameterVariable>(&literal.variable);
             p) {
    variable =
        parameters_[p->action_handle][p->parameter_index][p->constant_index];
  } else if (const HelperVariable *p =
                 std::get_if<HelperVariable>(&literal.variable)) {
    variable = helpers_[p->value];
  } else {
    assert(false);
  }
  if (variable == DONTCARE) {
    return static_cast<int>(SAT);
  }
  if (variable == SAT || variable == UNSAT) {
    return (literal.negated ? -1 : 1) * static_cast<int>(variable);
  }
  return (literal.negated ? -1 : 1) *
         static_cast<int>(variable + step * num_vars_);
}

planning::Plan ForeachEncoder::extract_plan(const sat::Model &model,
                                            unsigned int step) const noexcept {
  planning::Plan plan;
  for (unsigned int s = 0; s < step; ++s) {
    for (model::ActionHandle action_handle = 0;
         action_handle < support_.get_num_actions(); ++action_handle) {
      if (model[actions_[action_handle] + s * num_vars_]) {
        model::Action action = support_.get_problem().actions[action_handle];
        for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
             ++parameter_pos) {
          auto &parameter = action.parameters[parameter_pos];
          if (parameter.constant) {
            continue;
          }
          for (size_t i = 0;
               i < support_.get_constants_of_type(parameter.type).size(); ++i) {
            if (model[parameters_[action_handle][parameter_pos][i] +
                      s * num_vars_]) {
              parameter.constant =
                  support_.get_constants_of_type(parameter.type)[i];
              break;
            }
          }
          assert(parameter.constant);
        }
        assert(model::is_grounded(action));
        plan.push_back(std::move(action));
      }
    }
  }
  return plan;
}

void ForeachEncoder::encode_initial_state() noexcept {
  for (model::PredicateHandle predicate_handle = 0;
       predicate_handle < support_.get_num_predicates(); ++predicate_handle) {
    for (const auto &[ground_predicate, ground_predicate_handle] :
         support_.get_ground_predicates(predicate_handle)) {
      auto literal =
          Literal{PredicateVariable{predicate_handle, ground_predicate_handle, true},
                  !support_.is_init(ground_predicate)};
      initial_state_ << literal << sat::EndClause;
    }
  }
}

void ForeachEncoder::encode_actions() {
  for (model::ActionHandle action_handle = 0;
       action_handle < support_.get_problem().actions.size(); ++action_handle) {
    const auto &action = support_.get_problem().actions[action_handle];
    for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
         ++parameter_pos) {
      const auto &parameter = action.parameters[parameter_pos];
      if (parameter.constant) {
        continue;
      }
      size_t number_arguments =
          support_.get_constants_of_type(parameter.type).size();
      std::vector<Variable> all_arguments;
      all_arguments.reserve(number_arguments);
      auto action_var = ActionVariable{action_handle};
      for (size_t constant_index = 0; constant_index < number_arguments;
           ++constant_index) {
        auto parameter_var =
            ParameterVariable{action_handle, parameter_pos, constant_index};
        universal_clauses_ << Literal{parameter_var, true};
        universal_clauses_ << Literal{action_var, false};
        universal_clauses_ << sat::EndClause;
        all_arguments.push_back(parameter_var);
      }
      universal_clauses_ << Literal{action_var, true};
      for (const auto &argument : all_arguments) {
        universal_clauses_ << Literal{argument, false};
      }
      universal_clauses_ << sat::EndClause;
      universal_clauses_.at_most_one(all_arguments);
    }
  }
}

void ForeachEncoder::parameter_implies_predicate(bool is_negated,
                                                 bool is_effect) {
  const auto &predicate_support =
      support_.get_predicate_support(is_negated, is_effect);
  auto &formula = is_effect ? transition_clauses_ : universal_clauses_;
  for (model::PredicateHandle predicate_handle = 0;
       predicate_handle < support_.get_num_predicates(); ++predicate_handle) {
    for (const auto &[ground_predicate, ground_predicate_handle] :
         support_.get_ground_predicates(predicate_handle)) {
      for (const auto &[action_handle, assignments] :
           predicate_support[predicate_handle][ground_predicate_handle]) {
        for (const auto &assignment : assignments) {
          if (assignment.arguments.empty()) {
            formula << Literal{ActionVariable{action_handle}, true};
          } else {
            for (auto [parameter_index, constant_index] :
                 assignment.arguments) {
              formula << Literal{ParameterVariable{action_handle, parameter_index,
                                                   constant_index},
                                 true};
            }
          }
          formula << Literal{PredicateVariable{predicate_handle,
                                               ground_predicate_handle,
                                               !is_effect},
                             is_negated}
                  << sat::EndClause;
        }
      }
    }
  }
}

void ForeachEncoder::interference(bool is_negated) {
  const auto &precondition_support =
      support_.get_predicate_support(is_negated, false);
  const auto &effect_support =
      support_.get_predicate_support(!is_negated, true);
  for (model::PredicateHandle predicate_handle = 0;
       predicate_handle < support_.get_num_predicates(); ++predicate_handle) {
    for (const auto &[ground_predicate, ground_predicate_handle] :
         support_.get_ground_predicates(predicate_handle)) {
      for (const auto &[p_action_handle, p_assignments] :
           precondition_support[predicate_handle][ground_predicate_handle]) {
        for (const auto &[e_action_handle, e_assignments] :
             effect_support[predicate_handle][ground_predicate_handle]) {
          if (p_action_handle == e_action_handle) {
            continue;
          }
          for (const auto &p_assignment : p_assignments) {
            for (const auto &e_assignment : e_assignments) {
              if (p_assignment.arguments.empty()) {
                universal_clauses_
                    << Literal{ActionVariable{p_action_handle}, true};
              } else {
                for (auto [parameter_index, constant_index] :
                     p_assignment.arguments) {
                  universal_clauses_ << Literal{
                      ParameterVariable{p_action_handle, parameter_index,
                                        constant_index},
                      true};
                }
              }
              if (e_assignment.arguments.empty()) {
                universal_clauses_
                    << Literal{ActionVariable{e_action_handle}, true};
              } else {
                for (auto [parameter_index, constant_index] :
                     e_assignment.arguments) {
                  universal_clauses_ << Literal{
                      ParameterVariable{e_action_handle, parameter_index,
                                        constant_index},
                      true};
                }
              }
              universal_clauses_ << sat::EndClause;
            }
          }
        }
      }
    }
  }
}

void ForeachEncoder::frame_axioms(bool is_negated, size_t dnf_threshold) {
  for (model::PredicateHandle predicate_handle = 0;
       predicate_handle < support_.get_num_predicates(); ++predicate_handle) {
    for (const auto &[ground_predicate, ground_predicate_handle] :
         support_.get_ground_predicates(predicate_handle)) {

      const auto &predicate_support = support_.get_predicate_support(
          is_negated, true)[predicate_handle][ground_predicate_handle];

      size_t num_nontrivial_clauses = 0;
      for (const auto &[action_handle, assignments] : predicate_support) {
        for (const auto &assignment : assignments) {
          if (assignment.arguments.size() > 1) {
            ++num_nontrivial_clauses;
          }
        }
      }

      Formula dnf;
      dnf << Literal{PredicateVariable{predicate_handle, ground_predicate_handle,
                                       true},
                     is_negated}
          << sat::EndClause;
      dnf << Literal{PredicateVariable{predicate_handle, ground_predicate_handle,
                                       false},
                     !is_negated}
          << sat::EndClause;
      for (const auto &[action_handle, assignments] : predicate_support) {
        for (const auto &assignment : assignments) {
          if (assignment.arguments.empty()) {
            dnf << Literal{ActionVariable{action_handle}, false};
          } else if (assignment.arguments.size() == 1 ||
                     num_nontrivial_clauses < dnf_threshold) {
            for (auto [parameter_index, constant_index] :
                 assignment.arguments) {
              dnf << Literal{ParameterVariable{action_handle, parameter_index,
                                               constant_index},
                             false};
            }
          } else {
            for (auto [parameter_index, constant_index] :
                 assignment.arguments) {
              universal_clauses_ << Literal{HelperVariable{num_helpers_}, true};
              universal_clauses_
                  << Literal{ParameterVariable{action_handle, parameter_index,
                                               constant_index},
                             false};
              universal_clauses_ << sat::EndClause;
            }
            dnf << Literal{HelperVariable{num_helpers_}, false};
            ++num_helpers_;
          }
          dnf << sat::EndClause;
        }
      }
      transition_clauses_.add_dnf(dnf);
    }
  }
}

void ForeachEncoder::assume_goal() {
  for (const auto &predicate : support_.get_problem().goal) {
    model::GroundPredicateHandle index =
        support_.get_predicate_index(model::GroundPredicate{predicate});
    goal_ << Literal{PredicateVariable{predicate.definition, index, true},
                     predicate.negated}
          << sat::EndClause;
  }
}

void ForeachEncoder::init_sat_vars() {
  PRINT_INFO("Initializing sat variables...");
  unsigned int variable_counter = UNSAT + 1;

  actions_.reserve(support_.get_problem().actions.size());
  parameters_.resize(support_.get_problem().actions.size());
  for (model::ActionHandle action_handle = 0;
       action_handle < support_.get_problem().actions.size(); ++action_handle) {
    const auto &action = support_.get_problem().actions[action_handle];
    /* LOG_DEBUG(logger, "%s: %u", */
    /*           model::to_string(action, support_.get_problem()).c_str(), */
    /*           variable_counter); */
    actions_.push_back(variable_counter++);

    parameters_[action_handle].resize(action.parameters.size());

    for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
         ++parameter_pos) {
      const auto &parameter = action.parameters[parameter_pos];
      if (parameter.constant) {
        continue;
      }
      parameters_[action_handle][parameter_pos].reserve(
          support_.get_constants_of_type(parameter.type).size());
      for (size_t i = 0;
           i < support_.get_constants_of_type(parameter.type).size(); ++i) {
        /* LOG_DEBUG(logger, "Parameter %lu, index %lu: %u", */
        /*           parameters_[action_handle][parameter_pos].size(), i, */
        /*           variable_counter); */
        parameters_[action_handle][parameter_pos].push_back(variable_counter++);
      }
    }
  }

  predicates_.resize(support_.get_num_predicates());
  for (model::PredicateHandle predicate_handle = 0;
       predicate_handle < support_.get_num_predicates(); ++predicate_handle) {
    predicates_[predicate_handle].resize(
        support_.get_ground_predicates(predicate_handle).size());
    for (const auto &[ground_predicate, ground_predicate_handle] :
         support_.get_ground_predicates(predicate_handle)) {
      if (support_.is_rigid(ground_predicate, false)) {
        /* assert(false); */
        predicates_[predicate_handle][ground_predicate_handle] = SAT;
      } else if (support_.is_rigid(ground_predicate, true)) {
        /* assert(false); */
        predicates_[predicate_handle][ground_predicate_handle] = UNSAT;
      } else {
        predicates_[predicate_handle][ground_predicate_handle] = variable_counter++;
      }
    }
  }
  helpers_.resize(num_helpers_);
  for (size_t i = 0; i < num_helpers_; ++i) {
    helpers_[i] = variable_counter++;
  }
  num_vars_ = variable_counter - 3;
  PRINT_INFO("Representation uses %u variables", num_vars_);
}

} // namespace encoding
