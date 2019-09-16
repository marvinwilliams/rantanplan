#ifndef ENCODING_HPP
#define ENCODING_HPP

#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "model/support.hpp"
#include "sat/formula.hpp"
#include "sat/ipasir_solver.hpp"
#include "util/combinatorics.hpp"
#include "util/logger.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace encoding {

static logging::Logger logger{"Encoding"};

struct ActionVariable {
  model::ActionPtr action_ptr;
};

struct PredicateVariable {
  model::GroundPredicatePtr predicate_ptr;
  bool this_step;
};

struct ParameterVariable {
  model::ActionPtr action_ptr;
  size_t parameter_index;
  size_t constant_index;
};

using Variable =
    std::variant<ActionVariable, PredicateVariable, ParameterVariable>;

class Encoder {

public:
  using Formula = sat::Formula<Variable>;
  using Literal = Formula::Literal;

  Encoder(const model::Problem &problem)
      : solver_{std::make_unique<sat::IpasirSolver>()}, problem_{problem},
        support_{problem} {}

  void encode_initial_state() {
    for (model::GroundPredicatePtr predicate_ptr = 0;
         predicate_ptr < support_.get_ground_predicates().size();
         ++predicate_ptr) {
      const model::GroundPredicate &predicate =
          support_.get_ground_predicates()[predicate_ptr];
      bool is_init = std::any_of(
          problem_.initial_state.cbegin(), problem_.initial_state.cend(),
          [&predicate](const auto &init_predicate) {
            if (init_predicate.negated) {
              // This does assume a non conflicting initial state
              return false;
            }
            return predicate == model::GroundPredicate(init_predicate);
          });
      initial_state_ << Literal{PredicateVariable{predicate_ptr, true},
                                !is_init}
                     << sat::EndClause;
    }
  }

  void encode_actions() {
    std::vector<Variable> all_actions;
    all_actions.reserve(problem_.actions.size());
    for (model::ActionPtr action_ptr = 0; action_ptr < problem_.actions.size();
         ++action_ptr) {
      all_actions.emplace_back(ActionVariable{action_ptr});
    }
    universal_clauses_.at_most_one(all_actions);

    for (model::ActionPtr action_ptr = 0; action_ptr < problem_.actions.size();
         ++action_ptr) {
      const auto &action = problem_.actions[action_ptr];
      for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
           ++parameter_pos) {
        const auto &parameter = action.parameters[parameter_pos];
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
    auto predicate_support =
        support_.get_predicate_support(is_negated, is_effect);
    auto &formula = is_effect ? transition_clauses_ : universal_clauses_;
    for (model::GroundPredicatePtr predicate_ptr = 0;
         predicate_ptr < support_.get_ground_predicates().size();
         ++predicate_ptr) {
      for (const auto &[action_ptr, assignment] :
           predicate_support[predicate_ptr]) {
        formula << Literal{ActionVariable{action_ptr}, true};
        for (auto [parameter_index, constant_index] :
             assignment.get_arguments()) {
          formula << Literal{
              ParameterVariable{action_ptr, parameter_index, constant_index},
              true};
        }
        formula << Literal{PredicateVariable{predicate_ptr, !is_effect},
                           is_negated}
                << sat::EndClause;
      }
    }
  }

  void frame_axioms(bool is_negated) {
    for (model::GroundPredicatePtr predicate_ptr = 0;
         predicate_ptr < support_.get_ground_predicates().size();
         ++predicate_ptr) {
      Formula dnf;
      dnf << Literal{PredicateVariable{predicate_ptr, false}, is_negated}
          << sat::EndClause;
      dnf << Literal{PredicateVariable{predicate_ptr, true}, !is_negated}
          << sat::EndClause;
      for (const auto &[action_ptr, assignment] :
           support_.get_predicate_support(is_negated, true)[predicate_ptr]) {
        dnf << Literal{ActionVariable{action_ptr}, false};
        for (auto [parameter_index, constant_index] :
             assignment.get_arguments()) {
          dnf << Literal{
              ParameterVariable{action_ptr, parameter_index, constant_index},
              false};
        }
        dnf << sat::EndClause;
      }
      transition_clauses_.add_dnf(dnf);
    }
  }

  void init_vars() {
    LOG_INFO(logger, "Initializing sat variables...");
    Variable::value_type variable_counter = SAT + 1;

    actions_.reserve(problem_.actions.size());
    parameters_.reserve(problem_.actions.size());
    for (const auto &action : problem_.actions) {
      actions_.push_back(variable_counter++);

      parameters_.emplace_back();
      parameters_.back().reserve(action.parameters.size());

      for (const auto &parameter : action.parameters) {
        parameters_.back().emplace_back();
        parameters_.back().back().reserve(
            problem_.constants_of_type[parameter.type].size());
        for (size_t i = 0;
             i < problem_.constants_of_type[parameter.type].size(); ++i) {
          parameters_.back().back().push_back(variable_counter++);
        }
      }
    }

    predicates_.reserve(problem_.ground_predicates.size());
    for (size_t i = 0; i < problem_.ground_predicates.size(); ++i) {
      predicates_.insert({ground_predicate, variable_counter++});
    }

    num_vars = variable_counter - 1;
    LOG_INFO(logger, "Representation uses %u variables", num_vars);
  }

  void encode() {
    encode_initial_state();
    encode_actions();
    parameter_implies_predicate(false, false);
    parameter_implies_predicate(false, true);
    parameter_implies_predicate(true, false);
    parameter_implies_predicate(true, true);
    frame_axioms(false);
    frame_axioms(true);
    init_sat_vars();
    /* solver_->solve(); */
  }

  void solve() {
    
  }

private:
  std::unique_ptr<sat::Solver> solver_;
  Formula initial_state_;
  Formula universal_clauses_;
  Formula transition_clauses_;
  const model::Problem &problem_;
  model::Support support_;
};

} // namespace encoding

#endif /* end of include guard: ENCODING_HPP */
