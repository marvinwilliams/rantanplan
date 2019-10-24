#include "encoding/foreach.hpp"
#include "logging/logging.hpp"

namespace encoding {

  logging::Logger ForeachEncoder::logger{"Foreach"};

ForeachEncoder::ForeachEncoder(const support::Support &support, const Config& config) noexcept
    : support_{support} {
  if (config.log_encoding) {
    logger.add_appender(logging::default_appender);
  }
  encode_initial_state();
  encode_actions();
  parameter_implies_predicate(false, false);
  parameter_implies_predicate(false, true);
  parameter_implies_predicate(true, false);
  parameter_implies_predicate(true, true);
  /* forbid_assignments(); */
  interference(false);
  interference(true);
  num_helpers_ = 0;
  frame_axioms(false, config.dnf_threshold);
  frame_axioms(true, config.dnf_threshold);
  assume_goal();
  init_sat_vars();
}

int ForeachEncoder::get_sat_var(Literal literal, unsigned int step) const {
  size_t variable = 0;
  if (const ActionVariable *p =
          std::get_if<ActionVariable>(&literal.variable);
      p) {
    variable = actions_[p->action_ptr];
  } else if (const PredicateVariable *p =
                 std::get_if<PredicateVariable>(&literal.variable);
             p) {
    variable = predicates_[p->predicate_ptr][p->ground_predicate_ptr];
    if (!p->this_step && variable > UNSAT) {
      variable += static_cast<size_t>(num_vars_);
    }
  } else if (const ParameterVariable *p =
                 std::get_if<ParameterVariable>(&literal.variable);
             p) {
    variable =
        parameters_[p->action_ptr][p->parameter_index][p->constant_index];
  } else if (const HelperVariable *p = std::get_if<HelperVariable>(&literal.variable)) {
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

planning::Plan ForeachEncoder::extract_plan(const sat::Model &model, unsigned int step) const
    noexcept {
  planning::Plan plan;
  for (unsigned int s = 0; s < step; ++s) {
    for (model::ActionPtr action_ptr = 0;
         action_ptr < support_.get_num_actions(); ++action_ptr) {
      if (model[actions_[action_ptr] + s * num_vars_]) {
        model::Action action = support_.get_problem().actions[action_ptr];
        for (size_t parameter_pos = 0;
             parameter_pos < action.parameters.size(); ++parameter_pos) {
          auto &parameter = action.parameters[parameter_pos];
          if (parameter.constant) {
            continue;
          }
          for (size_t i = 0;
               i < support_.get_constants_of_type(parameter.type).size();
               ++i) {
            if (model[parameters_[action_ptr][parameter_pos][i] +
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
  for (model::PredicatePtr predicate_ptr = 0;
       predicate_ptr < support_.get_num_predicates(); ++predicate_ptr) {
    for (const auto &[ground_predicate, ground_predicate_ptr] :
         support_.get_ground_predicates(predicate_ptr)) {
      auto literal = Literal{
          PredicateVariable{predicate_ptr, ground_predicate_ptr, true},
          !support_.is_init(ground_predicate)};
      initial_state_ << literal << sat::EndClause;
    }
  }
}

void ForeachEncoder::encode_actions() {
  for (model::ActionPtr action_ptr = 0;
       action_ptr < support_.get_problem().actions.size(); ++action_ptr) {
    const auto &action = support_.get_problem().actions[action_ptr];
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
      auto action_var = ActionVariable{action_ptr};
      for (size_t constant_index = 0; constant_index < number_arguments;
           ++constant_index) {
        auto parameter_var = ParameterVariable{action_ptr, parameter_pos, constant_index};
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

void ForeachEncoder::parameter_implies_predicate(bool is_negated, bool is_effect) {
  for (model::PredicatePtr predicate_ptr = 0;
       predicate_ptr < support_.get_num_predicates(); ++predicate_ptr) {
    const auto &predicate_support =
        support_.get_predicate_support(predicate_ptr, is_negated, is_effect);
    auto &formula = is_effect ? transition_clauses_ : universal_clauses_;
    for (const auto &[ground_predicate, ground_predicate_ptr] :
         support_.get_ground_predicates(predicate_ptr)) {
      for (const auto &[action_ptr, assignment] :
           predicate_support[ground_predicate_ptr]) {
        if (assignment.arguments.empty()) {
        formula << Literal{ActionVariable{action_ptr}, true};
        } else {
          for (auto [parameter_index, constant_index] : assignment.arguments) {
            formula << Literal{
                ParameterVariable{action_ptr, parameter_index, constant_index},
                true};
          }
        }
        formula << Literal{PredicateVariable{predicate_ptr,
                                             ground_predicate_ptr,
                                             !is_effect},
                           is_negated}
                << sat::EndClause;
      }
    }
  }
}

void ForeachEncoder::interference(bool is_negated) {
  for (model::PredicatePtr predicate_ptr = 0;
       predicate_ptr < support_.get_num_predicates(); ++predicate_ptr) {
    const auto &precondition_support =
        support_.get_predicate_support(predicate_ptr, is_negated, false);
    const auto &effect_support =
        support_.get_predicate_support(predicate_ptr, !is_negated, true);
    for (const auto &[ground_predicate, ground_predicate_ptr] :
         support_.get_ground_predicates(predicate_ptr)) {
      for (const auto &[p_action_ptr, p_assignment] :
           precondition_support[ground_predicate_ptr]) {
        for (const auto &[e_action_ptr, e_assignment] :
             effect_support[ground_predicate_ptr]) {
          if (p_action_ptr == e_action_ptr) {
            continue;
          }
          if (p_assignment.arguments.empty()) {
            universal_clauses_ << Literal{ActionVariable{p_action_ptr}, true};
          } else {
            for (auto [parameter_index, constant_index] :
                 p_assignment.arguments) {
              universal_clauses_
                  << Literal{ParameterVariable{p_action_ptr, parameter_index,
                                               constant_index},
                             true};
            }
          }
          if (e_assignment.arguments.empty()) {
            universal_clauses_ << Literal{ActionVariable{e_action_ptr}, true};
          } else {
            for (auto [parameter_index, constant_index] :
                 e_assignment.arguments) {
              universal_clauses_
                  << Literal{ParameterVariable{e_action_ptr, parameter_index,
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

void ForeachEncoder::frame_axioms(bool is_negated, unsigned int dnf_threshold) {
  for (model::PredicatePtr predicate_ptr = 0;
      predicate_ptr < support_.get_num_predicates(); ++predicate_ptr) {
    for (const auto &[ground_predicate, ground_predicate_ptr] :
        support_.get_ground_predicates(predicate_ptr)) {
      const auto& predicate_support = support_.get_predicate_support(predicate_ptr, is_negated,
          true)[ground_predicate_ptr];
      bool need_indirection =
        static_cast<size_t>(std::count_if(predicate_support.begin(),
              predicate_support.end(), [](const auto& s) { return
              s.second.arguments.size() > 1;})) > dnf_threshold;
      if (need_indirection) {
        LOG_DEBUG(logger, "Support for %s needs indirection", model::to_string(ground_predicate, support_.get_problem()).c_str());
        Formula frame_axiom;
        frame_axiom << Literal{PredicateVariable{predicate_ptr, ground_predicate_ptr,
          true},
            is_negated};
        frame_axiom << Literal{PredicateVariable{predicate_ptr, ground_predicate_ptr,
          false},
            !is_negated};
        for (const auto &[action_ptr, assignment] :
            predicate_support) {
          if (assignment.arguments.empty()) {
            frame_axiom << Literal{ActionVariable{action_ptr}, false};
          } else if (assignment.arguments.size() == 1) {
            const auto&[parameter_index, constant_index] = assignment.arguments[0];
            frame_axiom << Literal{ParameterVariable{action_ptr, parameter_index, constant_index}, false};
          } else {
            for (auto [parameter_index, constant_index] : assignment.arguments) {
              universal_clauses_ << Literal{HelperVariable{num_helpers_}, true};
              universal_clauses_ << Literal{
                ParameterVariable{action_ptr, parameter_index, constant_index},
                  false};
              universal_clauses_ << sat::EndClause;
            }
            frame_axiom << Literal{HelperVariable{num_helpers_}, false};
            ++num_helpers_;
          }
        }
        frame_axiom << sat::EndClause;
        transition_clauses_.add_formula(frame_axiom);
      } else {
        Formula dnf;
        dnf << Literal{PredicateVariable{predicate_ptr, ground_predicate_ptr,
          true},
            is_negated}
        << sat::EndClause;
        dnf << Literal{PredicateVariable{predicate_ptr, ground_predicate_ptr,
          false},
            !is_negated}
        << sat::EndClause;
        for (const auto &[action_ptr, assignment] :
            predicate_support) {
          if (assignment.arguments.empty()) {
            dnf << Literal{ActionVariable{action_ptr}, false};
          } else {
              for (auto [parameter_index, constant_index] : assignment.arguments) {
                dnf << Literal{
                  ParameterVariable{action_ptr, parameter_index, constant_index},
                    false};
              }
          }
          dnf << sat::EndClause;
        }
        transition_clauses_.add_dnf(dnf);
      }
    }
  }
}

void ForeachEncoder::assume_goal() {
  for (const auto &predicate : support_.get_problem().goal) {
    model::GroundPredicatePtr index =
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
  for (model::ActionPtr action_ptr = 0;
       action_ptr < support_.get_problem().actions.size(); ++action_ptr) {
    const auto &action = support_.get_problem().actions[action_ptr];
    /* LOG_DEBUG(logger, "%s: %u", */
    /*           model::to_string(action, support_.get_problem()).c_str(), */
    /*           variable_counter); */
    actions_.push_back(variable_counter++);

    parameters_[action_ptr].resize(action.parameters.size());

    for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
         ++parameter_pos) {
      const auto &parameter = action.parameters[parameter_pos];
      if (parameter.constant) {
        continue;
      }
      parameters_[action_ptr][parameter_pos].reserve(
          support_.get_constants_of_type(parameter.type).size());
      for (size_t i = 0;
           i < support_.get_constants_of_type(parameter.type).size(); ++i) {
        /* LOG_DEBUG(logger, "Parameter %lu, index %lu: %u", */
        /*           parameters_[action_ptr][parameter_pos].size(), i, */
        /*           variable_counter); */
        parameters_[action_ptr][parameter_pos].push_back(variable_counter++);
      }
    }
  }

  predicates_.resize(support_.get_num_predicates());
  for (model::PredicatePtr predicate_ptr = 0;
       predicate_ptr < support_.get_num_predicates(); ++predicate_ptr) {
    predicates_[predicate_ptr].resize(
        support_.get_ground_predicates(predicate_ptr).size());
    for (const auto &[ground_predicate, ground_predicate_ptr] :
         support_.get_ground_predicates(predicate_ptr)) {
      if (support_.is_rigid(ground_predicate, false)) {
        /* assert(false); */
        predicates_[predicate_ptr][ground_predicate_ptr] = SAT;
      } else if (support_.is_rigid(ground_predicate, true)) {
        /* assert(false); */
        predicates_[predicate_ptr][ground_predicate_ptr] = UNSAT;
      } else {
        predicates_[predicate_ptr][ground_predicate_ptr] = variable_counter++;
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
