#ifndef SINGLE_PARAMETER_REPRESENTATION_HPP
#define SINGLE_PARAMETER_REPRESENTATION_HPP

#include "model/model.hpp"
#include "sat/representation"
#include "util/logger.hpp"

namespace sat {

static logging::Logger logger{"Representation"};

class SingleParameterRepresentation : public Representation {
public:
  SingleParameterRepresentation(const model::Problem &problem)
      : Representation{problem} {
    init_vars();
  }

  Variable get_variable(const model::Action &action) const {
    size_t index = get_action_index(action);
    return actions_[index];
  }

  Variable get_variable(const model::GroundPredicate &predicate) const {
    size_t index = get_predicate_index(predicate);
    return predicates_[index];
  }

  // pair (i,j) denotes that parameter i has value j from constants_of_type
  Formula get_action_assignment(
      const model::Action &action,
      std::vector<std::pair<size_t, size_t>> assignment) const {
    Formula formula;
    size_t index = get_action_index(action);

    formula << !Literal{get_variable(action)};
    for (const auto &argument : assignment) {
      formula << !Literal{parameters_[index][argument.first][argument.second]};
    }
    return formula;
  }

  Formula exactly_one_parameter(const model::Action &action) {
    Formula formula;
    size_t index = get_action_index(action);
    for (const auto &parameter_pos : parameters_[index]) {
      formula << !Literal{get_variable(action)};
      for (auto argument : parameter_pos) {
        formula << Literal{argument};
      }
      formula << EndClause;
      formula.at_most_one(parameter_pos);
    }
    return formula;
  }

  size_t get_num_vars() const { return num_vars; }

private:
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

  std::vector<model::GroundPredicate>
  get_ground_predicates(const model::Action &action,
                        const model::PredicateEvaluation &predicate) {
    model::ArgumentMapping mapping{action, predicate};

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
    for (size_t i = 0; i < mapping.size(); ++i) {
      auto type = action.parameters[mapping.get_action_parameter(i)].type;
      number_arguments.push_back(problem.constants_of_type[type].size());
    }

    auto combinations{algorithm::all_combinations(number_arguments)};

    std::vector<ActionPredicateAssignment> result;
    result.reserve(combinations.size());

    for (const auto &combination : combinations) {
      std::vector<model::ConstantPtr> arguments(constant_arguments);
      for (size_t i = 0; i < mapping.size(); ++i) {
        auto type = parameters[mapping.get_action_parameter(i)].type;
        for (auto index : get_predicate_parameters(i)) {
          arguments[index] = problem_.constants_of_type[type][combination[i]];
        }
      }

      result.emplace_back(
          ArgumentAssignment{mapping, combination},
          GroundPredicate{predicate.definition, std::move(arguments)});
    }
    return result;
  }

  void set_support() {
    for (const auto &action : problem_.actions) {
      for (const auto &predicate : action.preconditions) {
        get_ground_predicates(action, predicate) {
        for (const auto &ground_predicate :
          size_t index = get_predicate_index(ground_predicate);
        }
      }
    }

    size_t get_predicate_index(const GroundPredicate &predicate) {
      auto position = std::find(problem_.ground_predicates.cbegin(),
                                problem_.ground_predicates.cend(), predicate);
      if (position == problem_.ground_predicates.cend()) {
        LOG_WARN(logger, "Predicate does not exist");
      }
      return static_cast<size_t>(
          std::distance(problem_.ground_predicates.cbegin(), position));
    }

    size_t get_action_index(const Action &action) {
      auto position =
          std::find(problem_.actions.cbegin(), problem_.actions.cend(), action);
      if (position == problem_.actions.cend()) {
        LOG_WARN(logger, "Action %s does not exist", action.name.c_str());
      }
      return static_cast<size_t>(
          std::distance(problem_.actions.cbegin(), position));
    }

    std::vector<Variable> predicates_;
    std::vector<Variable> actions_;
    std::vector<std::vector<std::vector<Variable>>> parameters_;
    std::vector<ArgumentAssignment> pos_effect_support_;
    std::vector<ArgumentAssignment> neg_effect_support_;
    std::vector<ArgumentAssignment> pos_precondition_support_;
    std::vector<ArgumentAssignment> neg_precondition_support_;

    size_t num_vars;
  };

} // namespace sat

#endif /* end of include guard: SINGLE_PARAMETER_REPRESENTATION_HPP */
