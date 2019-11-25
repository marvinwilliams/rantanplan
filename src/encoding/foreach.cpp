#include "encoding/foreach.hpp"
#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized_problem.hpp"
#include "model/support.hpp"
#include "planning/planner.hpp"

namespace encoding {

logging::Logger ForeachEncoder::logger{"Foreach"};

ForeachEncoder::ForeachEncoder(const normalized::Problem &problem,
                               const Config &config) noexcept
    : support_{problem} {
  num_helpers_ = 0;
  encode_init();
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
    variable = predicates_[p->handle];
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
    return (literal.positive ? 1 : -1) * static_cast<int>(variable);
  }
  return (literal.positive ? 1 : -1) *
         static_cast<int>(variable + step * num_vars_);
}

planning::Planner::Plan ForeachEncoder::extract_plan(const sat::Model &model,
                                                     unsigned int step) const
    noexcept {
  planning::Planner::Plan plan;
  for (unsigned int s = 0; s < step; ++s) {
    for (size_t i = 0; i < support_.get_num_actions(); ++i) {
      if (model[actions_[i] + s * num_vars_]) {
        const normalized::Action &action = support_.get_problem().actions[i];
        std::vector<normalized::ConstantHandle> constants;
        for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
             ++parameter_pos) {
          auto &parameter = action.parameters[parameter_pos];
          if (parameter.constant) {
            constants.push_back(normalized::ConstantHandle{parameter.index});
          } else {
            auto type_handle = normalized::TypeHandle{parameter.index};
            for (size_t j = 0;
                 j < support_.get_constants_of_type(type_handle).size(); ++j) {
              if (model[parameters_[i][parameter_pos][j] + s * num_vars_]) {
                constants.push_back(
                    support_.get_constants_of_type(type_handle)[j]);
                break;
              }
            }
          }
          assert(constants.size() == parameter_pos + 1);
        }
        /* assert(normalized::is_grounded(action)); */
        plan.emplace_back(normalized::ActionHandle{i}, std::move(constants));
      }
    }
  }
  return plan;
}

void ForeachEncoder::encode_init() noexcept {
  /* for (normalized::PredicateHandle predicate_handle{0}; */
  /*      predicate_handle < support_.get_num_predicates(); ++predicate_handle)
   * { */
  for (const auto &[instantiation, handle] : support_.get_instantiations()) {
    auto literal =
        Literal{PredicateVariable{handle, true}, support_.is_init(handle)};
    init_ << literal << sat::EndClause;
    /* } */
  }
}

void ForeachEncoder::encode_actions() {
  for (normalized::ActionHandle action_handle{0};
       action_handle < support_.get_problem().actions.size(); ++action_handle) {
    const auto &action = support_.get_problem().actions[action_handle];
    for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
         ++parameter_pos) {
      const auto &parameter = action.parameters[parameter_pos];
      if (parameter.constant) {
        continue;
      }
      size_t number_arguments =
          support_
              .get_constants_of_type(normalized::TypeHandle{parameter.index})
              .size();
      std::vector<Variable> all_arguments;
      all_arguments.reserve(number_arguments);
      auto action_var = ActionVariable{action_handle};
      for (size_t constant_index = 0; constant_index < number_arguments;
           ++constant_index) {
        auto parameter_var =
            ParameterVariable{action_handle, parameter_pos, constant_index};
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

void ForeachEncoder::parameter_implies_predicate(bool positive,
                                                 bool is_effect) {
  const auto &predicate_support =
      support_.get_predicate_support(positive, is_effect);
  auto &formula = is_effect ? transition_clauses_ : universal_clauses_;
  for (const auto &[instantiation, instantiation_handle] :
       support_.get_instantiations()) {
    for (const auto &[action_handle, assignments] :
         predicate_support[instantiation_handle]) {
      for (const auto &assignment : assignments) {
        if (assignment.empty()) {
          formula << Literal{ActionVariable{action_handle}, false};
        } else {
          for (auto [parameter_index, constant_handle] : assignment) {
            assert(!support_.get_problem()
                        .actions[action_handle]
                        .parameters[parameter_index]
                        .constant);

            const auto &constants = support_.get_constants_of_type(
                normalized::TypeHandle{support_.get_problem()
                                           .actions[action_handle]
                                           .parameters[parameter_index]
                                           .index});
            auto it =
                std::find(constants.begin(), constants.end(), constant_handle);
            assert(it != constants.end());
            formula << Literal{
                ParameterVariable{
                    action_handle, parameter_index,
                    static_cast<size_t>(std::distance(constants.begin(), it))},
                false};
          }
        }
        formula << Literal{PredicateVariable{instantiation_handle, !is_effect},
                           positive}
                << sat::EndClause;
      }
    }
  }
}

void ForeachEncoder::interference(bool positive) {
  const auto &precondition_support =
      support_.get_predicate_support(positive, false);
  const auto &effect_support = support_.get_predicate_support(!positive, true);
  for (const auto &[instantiation, handle] : support_.get_instantiations()) {
    for (const auto &[p_action_handle, p_assignments] :
         precondition_support[handle]) {
      for (const auto &[e_action_handle, e_assignments] :
           effect_support[handle]) {
        if (p_action_handle == e_action_handle) {
          continue;
        }
        for (const auto &p_assignment : p_assignments) {
          for (const auto &e_assignment : e_assignments) {
            if (p_assignment.empty()) {
              universal_clauses_
                  << Literal{ActionVariable{p_action_handle}, false};
            } else {
              for (auto [parameter_index, constant_handle] : p_assignment) {
                assert(!support_.get_problem()
                            .actions[p_action_handle]
                            .parameters[parameter_index]
                            .constant);

                const auto &constants = support_.get_constants_of_type(
                    normalized::TypeHandle{support_.get_problem()
                                               .actions[p_action_handle]
                                               .parameters[parameter_index]
                                               .index});
                auto it = std::find(constants.begin(), constants.end(),
                                    constant_handle);
                assert(it != constants.end());
                universal_clauses_ << Literal{
                    ParameterVariable{p_action_handle, parameter_index,
                                      static_cast<size_t>(std::distance(
                                          constants.begin(), it))},
                    false};
              }
            }
            if (e_assignment.empty()) {
              universal_clauses_
                  << Literal{ActionVariable{e_action_handle}, false};
            } else {
              for (auto [parameter_index, constant_handle] : e_assignment) {
                assert(!support_.get_problem()
                            .actions[e_action_handle]
                            .parameters[parameter_index]
                            .constant);

                const auto &constants = support_.get_constants_of_type(
                    normalized::TypeHandle{support_.get_problem()
                                               .actions[e_action_handle]
                                               .parameters[parameter_index]
                                               .index});
                auto it = std::find(constants.begin(), constants.end(),
                                    constant_handle);
                assert(it != constants.end());
                universal_clauses_ << Literal{
                    ParameterVariable{e_action_handle, parameter_index,
                                      static_cast<size_t>(std::distance(
                                          constants.begin(), it))},
                    false};
              }
            }
            universal_clauses_ << sat::EndClause;
          }
        }
      }
    }
  }
}

void ForeachEncoder::frame_axioms(bool positive, size_t dnf_threshold) {
  for (const auto &[instantiation, handle] : support_.get_instantiations()) {
    const auto &predicate_support =
        support_.get_predicate_support(positive, true)[handle];

    size_t num_nontrivial_clauses = 0;
    for (const auto &[action_handle, assignments] : predicate_support) {
      for (const auto &assignment : assignments) {
        if (assignment.size() > 1) {
          ++num_nontrivial_clauses;
        }
      }
    }

    Formula dnf;
    dnf << Literal{PredicateVariable{handle, true}, positive} << sat::EndClause;
    dnf << Literal{PredicateVariable{handle, false}, !positive}
        << sat::EndClause;
    for (const auto &[action_handle, assignments] : predicate_support) {
      for (const auto &assignment : assignments) {
        if (assignment.empty()) {
          dnf << Literal{ActionVariable{action_handle}, true};
        } else if (assignment.size() == 1 ||
                   num_nontrivial_clauses < dnf_threshold) {
          for (auto [parameter_index, constant_handle] : assignment) {
            assert(!support_.get_problem()
                        .actions[action_handle]
                        .parameters[parameter_index]
                        .constant);

            const auto &constants = support_.get_constants_of_type(
                normalized::TypeHandle{support_.get_problem()
                                           .actions[action_handle]
                                           .parameters[parameter_index]
                                           .index});
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
            assert(!support_.get_problem()
                        .actions[action_handle]
                        .parameters[parameter_index]
                        .constant);

            const auto &constants = support_.get_constants_of_type(
                normalized::TypeHandle{support_.get_problem()
                                           .actions[action_handle]
                                           .parameters[parameter_index]
                                           .index});
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
          ++num_helpers_;
        }
        dnf << sat::EndClause;
      }
    }
    transition_clauses_.add_dnf(dnf);
  }
}

void ForeachEncoder::assume_goal() {
  for (const auto &[goal, positive] : support_.get_problem().goal) {
    goal_ << Literal{PredicateVariable{support_.get_instantiations().at(goal),
                                       true},
                     positive}
          << sat::EndClause;
  }
}

void ForeachEncoder::init_sat_vars() {
  PRINT_INFO("Initializing sat variables...");
  unsigned int variable_counter = UNSAT + 1;

  actions_.reserve(support_.get_problem().actions.size());
  parameters_.resize(support_.get_problem().actions.size());
  for (size_t i = 0; i < support_.get_problem().actions.size(); ++i) {
    const auto &action = support_.get_problem().actions[i];
    /* LOG_DEBUG(logger, "%s: %u", */
    /*           normalized::to_string(action, support_.get_problem()).c_str(),
     */
    /*           variable_counter); */
    actions_.push_back(variable_counter++);

    parameters_[i].resize(action.parameters.size());

    for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
         ++parameter_pos) {
      const auto &parameter = action.parameters[parameter_pos];
      if (parameter.constant) {
        continue;
      }
      auto handle = normalized::TypeHandle{parameter.index};
      parameters_[i][parameter_pos].reserve(
          support_.get_constants_of_type(handle).size());
      for (size_t j = 0; j < support_.get_constants_of_type(handle).size();
           ++j) {
        /* LOG_DEBUG(logger, "Parameter %lu, index %lu: %u", */
        /*           parameters_[action_handle][parameter_pos].size(), i, */
        /*           variable_counter); */
        parameters_[i][parameter_pos].push_back(variable_counter++);
      }
    }
  }

  predicates_.resize(support_.get_instantiations().size());
  for (const auto &[instantiation, handle] : support_.get_instantiations()) {
    if (support_.is_rigid(handle, true)) {
      /* assert(false); */
      predicates_[handle] = SAT;
    } else if (support_.is_rigid(handle, false)) {
      /* assert(false); */
      predicates_[handle] = UNSAT;
    } else {
      predicates_[handle] = variable_counter++;
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
