#ifndef ENCODING_HPP
#define ENCODING_HPP

#include "algorithm"
#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "sat/formula.hpp"
#include "sat/ipasir_solver.hpp"
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

class Encoder {

public:
  Encoder(const model::Problem &problem)
      : problem_{problem}, solver_{std::make_unique<sat::IpasirSolver>()} {}

  void encode_initial_state(const sat::Reprensentation &representation) {
    for (const auto &predicate : problem_.ground_predicates) {
      bool is_init = std::any_of(
          problem_.initial_state.cbegin(), problem_.initial_state.cend(),
          [&predicate](const auto &init_predicate) {
            if (init_predicate.negated) {
              return false;
            }
            return predicate.first == to_ground_predicate(init_predicate);
          });
      initial_state_ << sat::Literal{representation.get_variable(predicate),
                                     !is_init}
                     << sat::EndClause;
    }
  }

  void parameter_implies_predicate(const model::Action &action,
                                   const model::PredicateEvaluation &predicate,
                                   bool this_step) {
    const model::Problem &problem = representation.get_problem();
    auto mapping =
        model::get_argument_mapping(action.parameters, predicate.arguments);

    std::vector<model::ConstantPtr> constant_arguments;
    constant_arguments.resize(predicate.arguments.size());
    for (size_t i = 0; i < predicate.arguments.size(); ++i) {
      auto constant = std::get_if<model::ConstantPtr>(&predicate.arguments[i]);
      if (constant) {
        constant_arguments[i] = *constant;
      }
    }

    std::vector<size_t> number_arguments;
    number_arguments.reserve(mapping.size());
    for (const auto &parameter_mapping : mapping) {
      auto type = action.parameters[parameter_mapping.first].type;
      number_arguments.push_back(problem.constants_of_type[type].size());
    }

    auto combinations{algorithm::all_combinations(number_arguments)};

    for (const auto &combination : combinations) {
      std::vector<model::ConstantPtr> arguments(constant_arguments);
      for (size_t i = 0; i < mapping.size(); ++i) {
        auto type = parameters[mapping[i].first].type;
        for (auto index : mapping[i].second) {
          arguments[index] = problem_.constants_of_type[type][combination[i]];
        }
      }

      model::GroundPredicate ground_predicate{predicate.definition,
                                              std::move(arguments)};

      std::vector<std::pair<size_t, size_t>> assignment;
      assignment.reserve(mapping.size());
      for (size_t i = 0; i < mapping.size(); ++i) {
        assignment.emplace_back(mapping[i].first, combination[i]);
      }
      sat::Formula &formula =
          this_step ? universal_clauses_ : transition_clauses_;
      formula.add_formula(get_action_assignment(action, assignment));
      if (this_step) {
        formula << Literal{current_representation().get_variable{predicate}};
      } else {
        formula << Literal{current_representation().num_vars +
                           next_representation().get_variable{predicate}};
      }
      formula << sat::EndClause;
    }
  }

  void encode_actions() {
    std::vector<sat::Literal> all_actions;
    all_actions.reserve(problem_.actions.size());
    for (const auto &action : problem_.actions) {
      all_actions.emplace_back(current_representation().get_variable(action));
      universal_clauses_.add_formula(
          current_representation().exactly_one_parameter(action));
    }
    universal_clauses_.at_most_one(all_actions);

    for (const auto &action : problem_.actions) {
      for (const auto &predicate : action.preconditions) {
        parameter_implies_predicate(action, predicate, true);
      }
      for (const auto &predicate : action.effects) {
        parameter_implies_predicate(action_index, predicate, false);
      }
    }
  }

  void encode() {
    SingleParameterRepresentation representation{problem_};
    encode_initial_state(representation);

    solver_->solve();
  }

protected:
  std::unique_ptr<sat::Solver> solver_;
  sat::Formula initial_state_;
  sat::Formula universal_clauses_;
  sat::Formula transition_clauses_;
  model::Problem problem_;
};

} // namespace encoding

#endif /* end of include guard: ENCODING_HPP */
