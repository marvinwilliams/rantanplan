#ifndef FOREACH_HPP
#define FOREACH_HPP

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

extern logging::Logger logger;

class ForeachEncoder : public Encoder {

public:
  using Variable =
      std::variant<ActionVariable, PredicateVariable, ParameterVariable>;
  using Formula = sat::Formula<Variable>;
  using Literal = Formula::Literal;

  struct Representation {
    static constexpr unsigned int DONTCARE = 0;
    static constexpr unsigned int SAT = 1;
    static constexpr unsigned int UNSAT = 2;
    std::vector<unsigned int> predicates;
    std::vector<unsigned int> actions;
    std::vector<std::vector<std::vector<unsigned int>>> parameters;
    size_t num_vars;
  };

  explicit ForeachEncoder(model::Support &support) : Encoder{support} {}

  void plan(const Config &config) override {
    auto solver = get_solver(config);
    if (!solver) {
      PRINT_ERROR("Unknown solver type \'%s\'", config.encoder.c_str());
    }

    if (std::any_of(support_.get_problem().goal.begin(),
                    support_.get_problem().goal.end(), [this](const auto &p) {
                      return support_.is_rigid(p.definition) &&
                             support_.is_init(model::GroundPredicate{p}) ==
                                 p.negated;
                    })) {
      PRINT_INFO("Problem is trivially unsolvable");
      return;
    }
    *solver_ << static_cast<int>(representation_.SAT) << 0;
    *solver_ << -static_cast<int>(representation_.UNSAT) << 0;
    LOG_DEBUG(logger, "Initial state");
    add_formula(initial_state_, 0);
    LOG_DEBUG(logger, "Universal clauses");
    add_formula(universal_clauses_, 0);
    LOG_DEBUG(logger, "Transition clauses");
    add_formula(transition_clauses_, 0);
    unsigned int step = 1;
    while (true) {
      LOG_DEBUG(logger, "Universal clauses");
      add_formula(universal_clauses_, step);
      PRINT_INFO("Solving step %u", step);
      LOG_DEBUG(logger, "Goal clauses");
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
      LOG_DEBUG(logger, "Transition clauses");
      add_formula(transition_clauses_, step);
      ++step;
    }
  }

private:
  void encode_initial_state() {
    for (const auto &[predicate, predicate_ptr] :
         support_.get_ground_predicates()) {
      initial_state_ << Literal{PredicateVariable{predicate_ptr, true},
                                !support_.is_init(predicate)}
                     << sat::EndClause;
    }
  }

  void encode_actions() {
    for (model::ActionPtr action_ptr = 0;
         action_ptr < support_.get_problem().actions.size(); ++action_ptr) {
      const auto &action = support_.get_problem().actions[action_ptr];
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
    const auto &predicate_support =
        support_.get_predicate_support(is_negated, is_effect);
    auto &formula = is_effect ? transition_clauses_ : universal_clauses_;
    for (const auto &[predicate, predicate_ptr] :
         support_.get_ground_predicates()) {
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

  void forbid_assignments() noexcept {
    for (const auto &[action_ptr, assignment] :
         support_.get_forbidden_assignments()) {
      universal_clauses_ << Literal{ActionVariable{action_ptr}, true};
      for (auto [parameter_index, constant_index] :
           assignment.get_arguments()) {
        universal_clauses_ << Literal{
            ParameterVariable{action_ptr, parameter_index, constant_index},
            true};
      }
      universal_clauses_ << sat::EndClause;
    }
  }

  void interference(bool is_negated) {
    const auto &precondition_support =
        support_.get_predicate_support(is_negated, false);
    const auto &effect_support =
        support_.get_predicate_support(!is_negated, true);
    for (const auto &[predicate, predicate_ptr] :
         support_.get_ground_predicates()) {
      for (const auto &[p_action_ptr, p_assignment] :
           precondition_support[predicate_ptr]) {
        for (const auto &[e_action_ptr, e_assignment] :
             effect_support[predicate_ptr]) {
          if (p_action_ptr == e_action_ptr) {
            continue;
          }
          universal_clauses_ << Literal{ActionVariable{p_action_ptr}, true};
          universal_clauses_ << Literal{ActionVariable{e_action_ptr}, true};
          for (auto [parameter_index, constant_index] :
               p_assignment.get_arguments()) {
            universal_clauses_
                << Literal{ParameterVariable{p_action_ptr, parameter_index,
                                             constant_index},
                           true};
          }
          for (auto [parameter_index, constant_index] :
               e_assignment.get_arguments()) {
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

  void frame_axioms(bool is_negated) {
    for (const auto &[predicate, predicate_ptr] :
         support_.get_ground_predicates()) {
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
    for (const auto &predicate : support_.get_problem().goal) {
      if (!support_.is_rigid(predicate.definition)) {
        // If it was rigid and unsatisfiable, the problem would be trivially
        // unsatisfiable, which is checked beforehand
        model::GroundPredicatePtr index =
            support_.get_predicate_index(model::GroundPredicate{predicate});
        goal_ << Literal{PredicateVariable{index, true}, predicate.negated}
              << sat::EndClause;
      }
    }
  }

  void init_vars_() override {
    PRINT_INFO("Initializing sat variables...");
    unsigned int variable_counter = representation_.UNSAT + 1;

    representation_.actions.reserve(support_.get_problem().actions.size());
    representation_.parameters.reserve(support_.get_problem().actions.size());
    for (const auto &action : support_.get_problem().actions) {
      LOG_DEBUG(logger, "%s: %u",
                model::to_string(action, support_.get_problem()).c_str(),
                variable_counter);
      representation_.actions.push_back(variable_counter++);

      representation_.parameters.emplace_back();
      representation_.parameters.back().reserve(action.parameters.size());

      for (const auto &parameter : action.parameters) {
        representation_.parameters.back().emplace_back();
        representation_.parameters.back().back().reserve(
            support_.get_constants_of_type(parameter.type).size());
        for (size_t i = 0;
             i < support_.get_constants_of_type(parameter.type).size(); ++i) {
          LOG_DEBUG(logger, "Parameter %lu, index %lu: %u",
                    representation_.parameters.back().size(), i,
                    variable_counter);
          representation_.parameters.back().back().push_back(
              variable_counter++);
        }
      }
    }

    representation_.predicates.resize(support_.get_ground_predicates().size());
    for (const auto &[predicate, predicate_ptr] :
         support_.get_ground_predicates()) {
      if (support_.is_rigid(predicate, false)) {
        /* assert(false); */
        representation_.predicates[predicate_ptr] = representation_.SAT;
      } else if (support_.is_rigid(predicate, true)) {
        /* assert(false); */
        representation_.predicates[predicate_ptr] = representation_.UNSAT;
      } else {
        representation_.predicates[predicate_ptr] = variable_counter++;
      }
    }

    representation_.num_vars = variable_counter - 3;
    PRINT_INFO("Representation uses %u variables", representation_.num_vars);
  }

  void generate_formula_() override {
    encode_initial_state();
    encode_actions();
    parameter_implies_predicate(false, false);
    parameter_implies_predicate(false, true);
    parameter_implies_predicate(true, false);
    parameter_implies_predicate(true, true);
    forbid_assignments();
    interference(false);
    interference(true);
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
        variable += static_cast<size_t>(representation_.num_vars);
      }
    } else if (const ParameterVariable *p =
                   std::get_if<ParameterVariable>(&literal.variable);
               p) {
      variable =
          representation_
              .parameters[p->action_ptr][p->parameter_index][p->constant_index];
    } else {
      assert(false);
    }
    if (variable == representation_.DONTCARE) {
      return static_cast<int>(representation_.SAT);
    }
    if (variable == representation_.SAT || variable == representation_.UNSAT) {
      return (literal.negated ? -1 : 1) * static_cast<int>(variable);
    }
    return (literal.negated ? -1 : 1) *
           static_cast<int>(variable + step * representation_.num_vars);
  }

  void add_formula(const Formula &formula, unsigned int step) {
    LOG_DEBUG(logger, "Formula:");
    for (const auto &clause : formula.clauses) {
      LOG_DEBUG(logger, "Clause:");
      for (const auto &literal : clause.literals) {
        LOG_DEBUG(logger, "%d", get_sat_var(literal, step));
        *solver_ << get_sat_var(literal, step);
      }
      LOG_DEBUG(logger, "\n");
      *solver_ << 0;
    }
  }

  Plan extract_plan(const sat::Model &model, unsigned int step) {
    Plan plan;
    for (unsigned int s = 0; s < step; ++s) {
      for (model::ActionPtr action_ptr = 0;
           action_ptr < support_.get_problem().actions.size(); ++action_ptr) {
        if (model[representation_.actions[action_ptr] +
                  s * representation_.num_vars]) {
          const auto &action = support_.get_problem().actions[action_ptr];
          std::vector<model::ConstantPtr> arguments;
          arguments.reserve(action.parameters.size());
          for (size_t parameter_pos = 0;
               parameter_pos < action.parameters.size(); ++parameter_pos) {
            [[maybe_unused]] bool found = false;
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
          for (const auto &[parameter_pos, argument] : action.arguments) {
            arguments[parameter_pos] = argument;
          }
          plan.emplace_back(action_ptr, std::move(arguments));
        }
      }
    }
    return plan;
  }

  std::string to_string(const Plan &plan) {
    std::stringstream ss;
    unsigned int step = 0;
    for (const auto &[action_ptr, arguments] : plan) {
      ss << step << ": " << '('
         << support_.get_problem().actions[action_ptr].name << ' ';
      for (auto it = arguments.cbegin(); it != arguments.cend(); ++it) {
        if (it != arguments.cbegin()) {
          ss << ' ';
        }
        ss << support_.get_problem().constants[*it].name;
      }
      ss << ')' << '\n';
      ++step;
    }
    return ss.str();
  }

  Representation representation_;
  Formula initial_state_;
  Formula universal_clauses_;
  Formula transition_clauses_;
  Formula goal_;
};

} // namespace encoding

#endif /* end of include guard: FOREACH_HPP */
