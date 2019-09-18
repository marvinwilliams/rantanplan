#ifndef ENCODING_HPP
#define ENCODING_HPP

#include "logging/logging.hpp"
#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "model/support.hpp"
#include "sat/formula.hpp"
#include "sat/ipasir_solver.hpp"
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

using namespace std::chrono_literals;

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
  using Plan =
      std::vector<std::pair<model::ActionPtr, std::vector<model::ConstantPtr>>>;

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
    PRINT_DEBUG("Support size: %u", predicate_support.size());
    auto &formula = is_effect ? transition_clauses_ : universal_clauses_;
    for (model::GroundPredicatePtr predicate_ptr = 0;
         predicate_ptr < support_.get_ground_predicates().size();
         ++predicate_ptr) {
      PRINT_DEBUG("Support for predicate %u: %u", predicate_ptr.i,
                  predicate_support[predicate_ptr].size());
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
      dnf << Literal{PredicateVariable{predicate_ptr, true}, is_negated}
          << sat::EndClause;
      dnf << Literal{PredicateVariable{predicate_ptr, false}, !is_negated}
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

  void assume_goal() {
    for (const auto &predicate : problem_.goal) {
      model::GroundPredicatePtr index =
          support_.get_predicate_index(model::GroundPredicate{predicate});
      goal_ << Literal{PredicateVariable{index, true}, predicate.negated}
            << sat::EndClause;
    }
  }

  void init_vars() {
    PRINT_INFO("Initializing sat variables...");
    unsigned int variable_counter = representation_.SAT + 1;

    representation_.actions.reserve(problem_.actions.size());
    representation_.parameters.reserve(problem_.actions.size());
    for (const auto &action : problem_.actions) {
      representation_.actions.push_back(variable_counter++);

      representation_.parameters.emplace_back();
      representation_.parameters.back().reserve(action.parameters.size());

      for (const auto &parameter : action.parameters) {
        representation_.parameters.back().emplace_back();
        representation_.parameters.back().back().reserve(
            support_.get_constants_of_type(parameter.type).size());
        for (size_t i = 0;
             i < support_.get_constants_of_type(parameter.type).size(); ++i) {
          representation_.parameters.back().back().push_back(
              variable_counter++);
        }
      }
    }

    representation_.predicates.reserve(support_.get_ground_predicates().size());
    for (size_t i = 0; i < support_.get_ground_predicates().size(); ++i) {
      representation_.predicates.push_back(variable_counter++);
    }

    representation_.num_vars = variable_counter - 2;
    PRINT_INFO("Representation uses %u variables", representation_.num_vars);
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
    assume_goal();
    init_vars();
  }

  int get_sat_var(Literal literal, unsigned int step) {
    unsigned int variable = 0;
    if (const ActionVariable *p =
            std::get_if<ActionVariable>(&literal.variable);
        p) {
      variable = representation_.actions[p->action_ptr];
    } else if (const PredicateVariable *p =
                   std::get_if<PredicateVariable>(&literal.variable);
               p) {
      variable = representation_.predicates[p->predicate_ptr] +
                 (p->this_step ? 0 : representation_.num_vars);
    } else if (const ParameterVariable *p =
                   std::get_if<ParameterVariable>(&literal.variable);
               p) {
      variable =
          representation_
              .parameters[p->action_ptr][p->parameter_index][p->constant_index];
    }
    assert(variable != 0);
    if (variable == 1) {
      LOG_DEBUG(logger, "%d ", literal.negated ? -1 : 1);
      return literal.negated ? -1 : 1;
    }
    LOG_DEBUG(logger, "%d ",
              (literal.negated ? -1 : 1) *
                  (variable + step * representation_.num_vars));
    return (literal.negated ? -1 : 1) *
           static_cast<int>(variable + step * representation_.num_vars);
  }

  void add_formula(const Formula &formula, unsigned int step) {
    for (const auto &clause : formula.clauses) {
      for (const auto &literal : clause.literals) {
        *solver_ << get_sat_var(literal, step);
      }
      *solver_ << 0;
      LOG_DEBUG(logger, "\n");
    }
  }

  void extract_plan(const sat::Model &model, unsigned int step) {
    Plan plan;
    for (unsigned int s = 0; s < step; ++s) {
      for (model::ActionPtr action_ptr = 0;
           action_ptr < problem_.actions.size(); ++action_ptr) {
        if (model[representation_.actions[action_ptr] +
                  s * representation_.num_vars]) {
          const auto &action = problem_.actions[action_ptr];
          std::vector<model::ConstantPtr> arguments;
          arguments.reserve(action.parameters.size());
          for (size_t parameter_pos = 0;
               parameter_pos < action.parameters.size(); ++parameter_pos) {
            bool found = false;
            const auto &parameter = action.parameters[parameter_pos];
            for (size_t i = 0;
                 i < support_.get_constants_of_type(parameter.type).size();
                 ++i) {
              if (model[representation_
                            .parameters[action_ptr][parameter_pos][i] +
                        s * representation_.num_vars]) {
                arguments.push_back(
                    support_.get_constants_of_type(parameter.type)[i]);
                found = true;
                break;
              }
            }
            assert(found);
          }
          plan.emplace_back(action_ptr, std::move(arguments));
          break;
        }
      }
    }
    std::ofstream s{"plan.txt"};
    s << to_string(plan);
  }

  std::string to_string(const Plan &plan) {
    std::stringstream ss;
    unsigned int step = 0;
    for (const auto &[action_ptr, arguments] : plan) {
      ss << step << ": " << '(' << problem_.actions[action_ptr].name << ' ';
      for (auto it = arguments.cbegin(); it != arguments.cend(); ++it) {
        if (it != arguments.cbegin()) {
          ss << ' ';
        }
        ss << problem_.constants[*it].name;
      }
      ss << ')' << '\n';
      ++step;
    }
    return ss.str();
  }

  void plan() {
    *solver_ << representation_.SAT << 0;
    LOG_DEBUG(logger, "Initial state");
    add_formula(initial_state_, 0);
    LOG_DEBUG(logger, "First universal clauses");
    add_formula(universal_clauses_, 0);
    LOG_DEBUG(logger, "First transition clauses");
    add_formula(transition_clauses_, 0);
    unsigned int step = 1;
    while (true) {
      LOG_DEBUG(logger, "Universal clauses");
      add_formula(universal_clauses_, step);
      PRINT_INFO("Solving step %u", step);
      LOG_DEBUG(logger, "Assume goal");
      for (const auto &clause : goal_.clauses) {
        for (const auto &literal : clause.literals) {
          solver_->assume(get_sat_var(literal, step));
        }
      }
      LOG_DEBUG(logger, "Start solving");
      auto model = solver_->solve();
      if (model) {
        PRINT_INFO("Found plan");
        extract_plan(*model, step);
        return;
      }
      LOG_DEBUG(logger, "Transition clauses");
      add_formula(transition_clauses_, step);
      ++step;
    }
  }

private:
  struct Representation {
    const unsigned int SAT = 1;
    std::vector<unsigned int> predicates;
    std::vector<unsigned int> actions;
    std::vector<std::vector<std::vector<unsigned int>>> parameters;
    size_t num_vars;
  };

  std::unique_ptr<sat::Solver> solver_;
  Formula initial_state_;
  Formula universal_clauses_;
  Formula transition_clauses_;
  Formula goal_;
  const model::Problem &problem_;
  model::Support support_;
  Representation representation_;
}; // namespace encoding

} // namespace encoding

#endif /* end of include guard: ENCODING_HPP */
