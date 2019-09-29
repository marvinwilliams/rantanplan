#ifndef SEQUENTIAL_3_HPP
#define SEQUENTIAL_3_HPP

#include "config.hpp"
#include "encoding/encoding.hpp"
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

class Sequential3Encoder : public Encoder {

public:
  using Variable =
      std::variant<ActionVariable, PredicateVariable, GlobalParameterVariable>;
  using Formula = sat::Formula<Variable>;
  using Literal = Formula::Literal;

  struct Representation {
    const unsigned int DONTCARE = 0;
    const unsigned int SAT = 1;
    const unsigned int UNSAT = 2;
    std::vector<unsigned int> predicates;
    std::vector<unsigned int> actions;
    std::vector<std::vector<unsigned int>> parameters;
    size_t num_vars;
  };

  explicit Sequential3Encoder(const model::Problem &problem)
      : Encoder{problem} {}

  void plan(const Config &config) override {
    *solver_ << static_cast<int>(representation_.SAT) << 0;
    *solver_ << -static_cast<int>(representation_.UNSAT) << 0;
    add_formula(initial_state_, 0);
    add_formula(universal_clauses_, 0);
    add_formula(transition_clauses_, 0);
    unsigned int step = 1;
    while (true) {
      add_formula(universal_clauses_, step);
      PRINT_INFO("Solving step %u", step);
      for (const auto &clause : goal_.clauses) {
        for (const auto &literal : clause.literals) {
          solver_->assume(get_sat_var(literal, step));
        }
      }
      auto model = solver_->solve();
      if (model) {
        PRINT_INFO("Found plan");
        auto plan = extract_plan(*model, step);
        if (config.plan_output_file != "") {
          std::ofstream s{config.plan_output_file};
          s << to_string(plan);
        } else {
          std::cout << to_string(plan);
        }
        return;
      }
      add_formula(transition_clauses_, step);
      ++step;
    }
  }

private:
  void encode_initial_state() {
    for (const auto &kv_pair : support_.get_ground_predicates()) {
      bool is_init = std::any_of(
          problem_.initial_state.cbegin(), problem_.initial_state.cend(),
          [&kv_pair](const auto &init_predicate) {
            if (init_predicate.negated) {
              // This does assume a non conflicting initial state
              return false;
            }
            return kv_pair.first == model::GroundPredicate(init_predicate);
          });
      initial_state_ << Literal{PredicateVariable{kv_pair.second, true},
                                !is_init}
                     << sat::EndClause;
    }
  }

  void encode_at_most_one_parameter() {
    for (size_t parameter_pos = 0; parameter_pos < max_parameters_;
         ++parameter_pos) {
      std::vector<Variable> all_arguments;
      all_arguments.reserve(problem_.constants.size());
      for (model::ConstantPtr constant_ptr = 0;
           constant_ptr < problem_.constants.size(); ++constant_ptr) {
        all_arguments.emplace_back(
            GlobalParameterVariable{parameter_pos, constant_ptr});
      }
      universal_clauses_.at_most_one(all_arguments);
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
        universal_clauses_ << Literal{ActionVariable{action_ptr}, true};
        for (model::ConstantPtr constant_ptr :
             support_.get_constants_of_type(parameter.type)) {
          universal_clauses_ << Literal{
              GlobalParameterVariable{parameter_pos, constant_ptr}, false};
        }
        universal_clauses_ << sat::EndClause;
      }
    }
  }

  void parameter_implies_predicate(bool is_negated, bool is_effect) {
    const auto &predicate_support =
        support_.get_predicate_support(is_negated, is_effect);
    auto &formula = is_effect ? transition_clauses_ : universal_clauses_;
    for (model::GroundPredicatePtr predicate_ptr = 0;
         predicate_ptr < support_.get_ground_predicates().size();
         ++predicate_ptr) {
      for (const auto &[action_ptr, assignment] :
           predicate_support[predicate_ptr]) {
        formula << Literal{ActionVariable{action_ptr}, true};
        const auto &parameters = problem_.actions[action_ptr].parameters;
        for (auto [parameter_index, constant_index] :
             assignment.get_arguments()) {
          const auto &constants_of_type =
              support_.get_constants_of_type(parameters[parameter_index].type);
          formula << Literal{
              GlobalParameterVariable{parameter_index,
                                      constants_of_type[constant_index]},
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
        const auto &parameters = problem_.actions[action_ptr].parameters;
        for (auto [parameter_index, constant_index] :
             assignment.get_arguments()) {
          const auto &constants_of_type =
              support_.get_constants_of_type(parameters[parameter_index].type);
          dnf << Literal{
              GlobalParameterVariable{parameter_index,
                                      constants_of_type[constant_index]},
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

  void init_vars_() override {
    PRINT_INFO("Initializing sat variables...");
    unsigned int variable_counter = representation_.UNSAT + 1;

    representation_.actions.reserve(problem_.actions.size());
    for (size_t i = 0; i < problem_.actions.size(); ++i) {
      representation_.actions.push_back(variable_counter++);
    }

    representation_.parameters.resize(max_parameters_);
    for (size_t i = 0; i < max_parameters_; ++i) {
      representation_.parameters[i].reserve(problem_.constants.size());
      for (size_t j = 0; j < problem_.constants.size(); ++j) {
        representation_.parameters[i].push_back(variable_counter++);
      }
    }

    representation_.predicates.reserve(support_.get_ground_predicates().size());
    for (size_t i = 0; i < support_.get_ground_predicates().size(); ++i) {
      if (!support_.is_relevant(i)) {
        representation_.predicates.push_back(representation_.DONTCARE);
      } else if (support_.is_rigid(i, false)) {
        representation_.predicates.push_back(representation_.SAT);
      } else if (support_.is_rigid(i, true)) {
        representation_.predicates.push_back(representation_.UNSAT);
      } else {
        representation_.predicates.push_back(variable_counter++);
      }
    }

    representation_.num_vars = variable_counter - 3;
    PRINT_INFO("Representation uses %u variables", representation_.num_vars);
  }

  void generate_formula_() override {
    max_parameters_ = 0;
    for (const auto &action : problem_.actions) {
      if (action.parameters.size() > max_parameters_) {
        max_parameters_ = action.parameters.size();
      }
    }
    encode_initial_state();
    encode_actions();
    encode_at_most_one_parameter();
    parameter_implies_predicate(false, false);
    parameter_implies_predicate(false, true);
    parameter_implies_predicate(true, false);
    parameter_implies_predicate(true, true);
    frame_axioms(false);
    frame_axioms(true);
    assume_goal();
  }

  int get_sat_var(Literal literal, unsigned int step) {
    size_t variable = 0;
    if (const ActionVariable *p =
            std::get_if<ActionVariable>(&literal.variable);
        p) {
      variable = representation_.actions[p->action_ptr];
    } else if (const PredicateVariable *p =
                   std::get_if<PredicateVariable>(&literal.variable);
               p) {
      variable = representation_.predicates[p->predicate_ptr];
      if (!p->this_step && variable > representation_.UNSAT) {
        variable += static_cast<unsigned int>(representation_.num_vars);
      }
    } else if (const GlobalParameterVariable *p =
                   std::get_if<GlobalParameterVariable>(&literal.variable);
               p) {
      variable =
          representation_.parameters[p->parameter_index][p->constant_index];
    } else {
      assert(false);
    }
    if (variable == representation_.DONTCARE) {
      return static_cast<int>(representation_.SAT);
    }
    if (variable == representation_.SAT || variable == representation_.UNSAT) {
      LOG_DEBUG(logger, "%d ", (literal.negated ? -1 : 1) * variable);
      return (literal.negated ? -1 : 1) * static_cast<int>(variable);
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
    }
  }

  Plan extract_plan(const sat::Model &model, unsigned int step) {
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
            [[maybe_unused]] bool found = false;
            for (model::ConstantPtr constant_ptr = 0;
                 constant_ptr < problem_.constants.size(); ++constant_ptr) {
              if (model[representation_
                            .parameters[parameter_pos][constant_ptr] +
                        s * representation_.num_vars]) {
                arguments.push_back(constant_ptr);
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
    return plan;
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

  size_t max_parameters_;
  Representation representation_;
  Formula initial_state_;
  Formula universal_clauses_;
  Formula transition_clauses_;
  Formula goal_;
};

} // namespace encoding

#endif /* end of include guard: SEQUENTIAL_3_HPP */
