#ifndef FOREACH_HPP
#define FOREACH_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "model/support.hpp"
#include "planning/planner.hpp"
#include "sat/formula.hpp"
#include "sat/model.hpp"
#include "util/combinatorics.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace encoding {

class ForeachEncoder {

public:
  static constexpr unsigned int DONTCARE = 0;
  static constexpr unsigned int SAT = 1;
  static constexpr unsigned int UNSAT = 2;

  inline static logging::Logger logger{"Foreach"};
  struct ActionVariable {
    model::ActionPtr action_ptr;
  };

  struct PredicateVariable {
    model::PredicatePtr predicate_ptr;
    model::GroundPredicatePtr ground_predicate_ptr;
    bool this_step;
  };

  struct ParameterVariable {
    model::ActionPtr action_ptr;
    size_t parameter_index;
    size_t constant_index;
  };

  using Variable =
      std::variant<ActionVariable, PredicateVariable, ParameterVariable>;
  using Formula = sat::Formula<Variable>;
  using Literal = Formula::Literal;

  explicit ForeachEncoder(const model::Support &support) noexcept
      : support_{support} {
    encode_initial_state();
    encode_actions();
    parameter_implies_predicate(false, false);
    parameter_implies_predicate(false, true);
    parameter_implies_predicate(true, false);
    parameter_implies_predicate(true, true);
    /* forbid_assignments(); */
    interference(false);
    interference(true);
    frame_axioms(false);
    frame_axioms(true);
    assume_goal();
    init_sat_vars();
  }

  int get_sat_var(Literal literal, unsigned int step) const {
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

  planning::Plan extract_plan(const sat::Model &model, unsigned int step) const
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

  const auto &get_initial_clauses() const noexcept { return initial_state_; }

  const auto &get_universal_clauses() const noexcept {
    return universal_clauses_;
  }

  const auto &get_transition_clauses() const noexcept {
    return transition_clauses_;
  }

  const auto &get_goal_clauses() const noexcept { return goal_; }

private:
  void encode_initial_state() noexcept {
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

  void encode_actions() {
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
        for (size_t constant_index = 0; constant_index < number_arguments;
             ++constant_index) {
          all_arguments.emplace_back(
              ParameterVariable{action_ptr, parameter_pos, constant_index});
        }
        universal_clauses_ << Literal{ActionVariable{action_ptr}, true};
        for (const auto &argument : all_arguments) {
          universal_clauses_ << Literal{argument, false};
        }
        universal_clauses_ << sat::EndClause;
        universal_clauses_.at_most_one(all_arguments);
      }
    }
  }

  void parameter_implies_predicate(bool is_negated, bool is_effect) {
    for (model::PredicatePtr predicate_ptr = 0;
         predicate_ptr < support_.get_num_predicates(); ++predicate_ptr) {
      const auto &predicate_support =
          support_.get_predicate_support(predicate_ptr, is_negated, is_effect);
      auto &formula = is_effect ? transition_clauses_ : universal_clauses_;
      for (const auto &[ground_predicate, ground_predicate_ptr] :
           support_.get_ground_predicates(predicate_ptr)) {
        for (const auto &[action_ptr, assignment] :
             predicate_support[ground_predicate_ptr]) {
          formula << Literal{ActionVariable{action_ptr}, true};
          for (auto [parameter_index, constant_index] : assignment.arguments) {
            formula << Literal{
                ParameterVariable{action_ptr, parameter_index, constant_index},
                true};
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

  /* void forbid_assignments() noexcept { */
  /*   for (const auto &[action_ptr, assignment] : */
  /*        support_.get_forbidden_assignments()) { */
  /*     universal_clauses_ << Literal{ActionVariable{action_ptr}, true}; */
  /*     for (auto [parameter_index, constant_index] : */
  /*          assignment.get_arguments()) { */
  /*       universal_clauses_ << Literal{ */
  /*           ParameterVariable{action_ptr, parameter_index, constant_index},
   */
  /*           true}; */
  /*     } */
  /*     universal_clauses_ << sat::EndClause; */
  /*   } */
  /* } */

  void interference(bool is_negated) {
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
            universal_clauses_ << Literal{ActionVariable{p_action_ptr}, true};
            universal_clauses_ << Literal{ActionVariable{e_action_ptr}, true};
            for (auto [parameter_index, constant_index] :
                 p_assignment.arguments) {
              universal_clauses_
                  << Literal{ParameterVariable{p_action_ptr, parameter_index,
                                               constant_index},
                             true};
            }
            for (auto [parameter_index, constant_index] :
                 e_assignment.arguments) {
              universal_clauses_
                  << Literal{ParameterVariable{e_action_ptr, parameter_index,
                                               constant_index},
                             true};
            }
            universal_clauses_ << sat::EndClause;
          }
        }
      }
    }
  }

  void frame_axioms(bool is_negated) {
    for (model::PredicatePtr predicate_ptr = 0;
         predicate_ptr < support_.get_num_predicates(); ++predicate_ptr) {
      for (const auto &[ground_predicate, ground_predicate_ptr] :
           support_.get_ground_predicates(predicate_ptr)) {
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
             support_.get_predicate_support(predicate_ptr, is_negated,
                                            true)[ground_predicate_ptr]) {
          dnf << Literal{ActionVariable{action_ptr}, false};
          for (auto [parameter_index, constant_index] : assignment.arguments) {
            dnf << Literal{
                ParameterVariable{action_ptr, parameter_index, constant_index},
                false};
          }
          dnf << sat::EndClause;
        }
        transition_clauses_.add_dnf(dnf);
      }
    }
  }

  void assume_goal() {
    for (const auto &predicate : support_.get_problem().goal) {
      model::GroundPredicatePtr index =
          support_.get_predicate_index(model::GroundPredicate{predicate});
      goal_ << Literal{PredicateVariable{predicate.definition, index, true},
                       predicate.negated}
            << sat::EndClause;
    }
  }

  void init_sat_vars() {
    PRINT_INFO("Initializing sat variables...");
    unsigned int variable_counter = UNSAT + 1;

    actions_.reserve(support_.get_problem().actions.size());
    parameters_.resize(support_.get_problem().actions.size());
    for (model::ActionPtr action_ptr = 0;
         action_ptr < support_.get_problem().actions.size(); ++action_ptr) {
      const auto &action = support_.get_problem().actions[action_ptr];
      LOG_DEBUG(logger, "%s: %u",
                model::to_string(action, support_.get_problem()).c_str(),
                variable_counter);
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
          LOG_DEBUG(logger, "Parameter %lu, index %lu: %u",
                    parameters_[action_ptr][parameter_pos].size(), i,
                    variable_counter);
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
    num_vars_ = variable_counter - 3;
    PRINT_INFO("Representation uses %u variables", num_vars_);
  }

  const model::Support &support_;
  size_t num_vars_;
  std::vector<std::vector<unsigned int>> predicates_;
  std::vector<unsigned int> actions_;
  std::vector<std::vector<std::vector<unsigned int>>> parameters_;
  Formula initial_state_;
  Formula universal_clauses_;
  Formula transition_clauses_;
  Formula goal_;
};

} // namespace encoding

#endif /* end of include guard: FOREACH_HPP */
