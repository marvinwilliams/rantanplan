#ifndef SUPPORT_HPP
#define SUPPORT_HPP

#include "model/model.hpp"
#include "model/model_utils.hpp"

#include <cassert>
#include <utility>
#include <variant>
#include <vector>

namespace model {

class ArgumentMapping {
public:
  ArgumentMapping(const std::vector<Parameter> &parameters,
                  const std::vector<Argument> &arguments) {
    for (size_t i = 0; i < parameters.size(); ++i) {
      std::vector<size_t> parameter_matches;
      for (size_t j = 0; j < arguments.size(); ++j) {
        auto parameter_ptr = std::get_if<ParameterPtr>(&arguments[j]);
        if (parameter_ptr && *parameter_ptr == i) {
          parameter_matches.push_back(j);
        }
      }
      if (!parameter_matches.empty()) {
        matches_.emplace_back(i, std::move(parameter_matches));
      }
    }
  }

  size_t size() const { return matches_.size(); }

  size_t get_parameter_index(size_t pos) const { return matches_[pos].first; }

  const std::vector<size_t> &get_argument_matches(size_t pos) const {
    return matches_[pos].second;
  }

private:
  std::vector<std::pair<size_t, std::vector<size_t>>> matches_;
};

class ArgumentAssignment {
public:
  ArgumentAssignment(const ArgumentMapping &mapping,
                     const std::vector<size_t> &arguments) {
    assert(arguments.size() == mapping.size());
    arguments_.reserve(mapping.size());
    for (size_t i = 0; i < mapping.size(); ++i) {
      arguments_.emplace_back(mapping.get_parameter_index(i), arguments[i]);
    }
  }

  const std::vector<std::pair<size_t, size_t>> &get_arguments() const {
    return arguments_;
  }

private:
  std::vector<std::pair<size_t, size_t>> arguments_;
};

class Support {
public:
  Support(const Problem &problem) {
    sort_constants(problem);
    ground_predicates(problem);
    set_predicate_support(problem);
  }

  const std::vector<ConstantPtr> &get_constants_of_type(TypePtr type) {
    return constants_of_type[type];
  }

  const std::vector<GroundPredicate> &get_ground_predicates() const {
    return ground_predicates_;
  }

  GroundPredicatePtr get_predicate_index(const GroundPredicate &predicate) {
    auto it = std::find(ground_predicates_.cbegin(), ground_predicates_.cend(),
                        predicate);
    if (it == ground_predicates_.cend()) {
      // Invalid predicate
      return ground_predicates_.size();
    }
    return std::distance(ground_predicates_.cbegin(), it);
  }

  const std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>> &
  get_predicate_support(bool is_negated, bool is_effect) {
    return select_support(is_negated, is_effect);
  }

private:
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>> &
  select_support(bool is_negated, bool is_effect) {
    if (is_negated) {
      return is_effect ? neg_effect_support_ : neg_precondition_support_;
    } else {
      return is_effect ? pos_effect_support_ : pos_precondition_support_;
    }
  }

  void sort_constants(const Problem &problem) {
    constants_of_type.resize(problem.types.size());
    for (ConstantPtr constant_ptr = 0; constant_ptr < problem.constants.size();
         ++constant_ptr) {
      TypePtr type = problem.constants[constant_ptr].type;
      constants_of_type[type].push_back(constant_ptr);
      while (problem.types[type].parent != type) {
        type = problem.types[type].parent;
        constants_of_type[type].push_back(constant_ptr);
      }
    }
  }

  void ground_predicates(const Problem &problem) {
    for (PredicatePtr predicate_ptr = 0;
         predicate_ptr < problem.predicates.size(); ++predicate_ptr) {
      const PredicateDefinition &predicate = problem.predicates[predicate_ptr];

      std::vector<size_t> number_arguments;
      number_arguments.reserve(predicate.parameters.size());
      for (const auto &parameter : predicate.parameters) {
        number_arguments.push_back(
            constants_of_type[parameter.type].size());
      }

      auto combinations = all_combinations(number_arguments);

      ground_predicates_.reserve(ground_predicates_.size() +
                                 combinations.size());
      for (const auto &combination : combinations) {
        std::vector<ConstantPtr> arguments;
        arguments.reserve(combination.size());
        for (size_t j = 0; j < combination.size(); ++j) {
          auto type = predicate.parameters[j].type;
          auto constant = constants_of_type[type][combination[j]];
          arguments.push_back(constant);
        }
        ground_predicates_.emplace_back(predicate_ptr, std::move(arguments));
      }
    }
  }

  void ground_action_predicate(const Problem &problem, ActionPtr action_ptr,
                               const PredicateEvaluation &predicate,
                               bool is_effect) {
    const auto &action = problem.actions[action_ptr];
    ArgumentMapping mapping{action.parameters, predicate.arguments};

    std::vector<model::ConstantPtr> arguments(predicate.arguments.size());
    for (size_t i = 0; i < predicate.arguments.size(); ++i) {
      auto constant = std::get_if<model::ConstantPtr>(&predicate.arguments[i]);
      if (constant) {
        arguments[i] = *constant;
      }
    }

    model::GroundPredicate ground_predicate{predicate.definition,
                                            std::move(arguments)};
    std::vector<size_t> number_arguments;
    number_arguments.reserve(mapping.size());
    for (size_t i = 0; i < mapping.size(); ++i) {
      auto type = action.parameters[mapping.get_parameter_index(i)].type;
      number_arguments.push_back(constants_of_type[type].size());
    }

    auto combinations{all_combinations(number_arguments)};

    auto predicate_support = select_support(predicate.negated, is_effect);

    for (const auto &combination : combinations) {
      for (size_t i = 0; i < mapping.size(); ++i) {
        auto type = action.parameters[mapping.get_parameter_index(i)].type;
        for (auto index : mapping.get_argument_matches(i)) {
          ground_predicate.arguments[index] =
              get_constants_of_type(type)[combination[i]];
        }
      }
      auto index = get_predicate_index(ground_predicate);
      ArgumentAssignment assignment{mapping, combination};
      predicate_support[index].emplace_back(action_ptr, assignment);
    }
  }

  void set_predicate_support(const Problem &problem) {
    pos_precondition_support_.resize(ground_predicates_.size());
    neg_precondition_support_.resize(ground_predicates_.size());
    pos_effect_support_.resize(ground_predicates_.size());
    neg_effect_support_.resize(ground_predicates_.size());
    for (ActionPtr action_ptr = 0; action_ptr < problem.actions.size();
         ++action_ptr) {
      for (const auto &predicate : problem.actions[action_ptr].preconditions) {
        ground_action_predicate(problem, action_ptr, predicate, false);
      }
      for (const auto &predicate : problem.actions[action_ptr].effects) {
        ground_action_predicate(problem, action_ptr, predicate, true);
      }
    }
  }

  std::vector<std::vector<ConstantPtr>> constants_of_type;
  std::vector<GroundPredicate> ground_predicates_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>
      pos_precondition_support_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>
      neg_precondition_support_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>
      pos_effect_support_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>
      neg_effect_support_;
};

} // namespace model

#endif /* end of include guard: SUPPORT_HPP */
