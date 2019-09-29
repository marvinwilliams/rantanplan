#ifndef SUPPORT_HPP
#define SUPPORT_HPP

#include "logging/logging.hpp"
#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "model/to_string.hpp"

#include <cassert>
#include <utility>
#include <variant>
#include <vector>

namespace model {

namespace support {

logging::Logger logger{"Support"};

struct ArgumentMapping {
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
        matches.emplace_back(i, std::move(parameter_matches));
      }
    }
  }

  size_t size() const { return matches.size(); }

  size_t get_parameter_index(size_t pos) const { return matches[pos].first; }

  const std::vector<size_t> &get_argument_matches(size_t pos) const {
    return matches[pos].second;
  }

  std::vector<std::pair<size_t, std::vector<size_t>>> matches;
};

struct ArgumentAssignment {
  ArgumentAssignment(const ArgumentMapping &mapping,
                     const std::vector<size_t> &arguments) {
    assert(arguments.size() == mapping.size());
    this->arguments.reserve(mapping.size());
    for (size_t i = 0; i < mapping.size(); ++i) {
      this->arguments.emplace_back(mapping.get_parameter_index(i),
                                   arguments[i]);
    }
  }

  const std::vector<std::pair<size_t, size_t>> &get_arguments() const {
    return arguments;
  }

  std::vector<std::pair<size_t, size_t>> arguments;
};

class Support {
public:
  explicit Support(const Problem &problem) {
    LOG_DEBUG(logger, "Sort constants by type...");
    sort_constants(problem);
    LOG_DEBUG(logger, "Ground all predicates...");
    ground_predicates(problem);
    LOG_DEBUG(logger, "The problem has %u grounded predicates",
              ground_predicates_.size());
    LOG_DEBUG(logger, "Compute predicate support...");
    set_predicate_support(problem);
    initial_state_.reserve(problem.initial_state.size());
    for (const auto &predicate : problem.initial_state) {
      auto predicate_ptr = get_predicate_index(GroundPredicate(predicate));
      initial_state_.push_back({predicate_ptr, predicate.negated});
    }
    goal_.reserve(problem.goal.size());
    for (const auto &predicate : problem.goal) {
      auto predicate_ptr = get_predicate_index(GroundPredicate{predicate});
      goal_.push_back({predicate_ptr, predicate.negated});
    }
  }

  const std::vector<ConstantPtr> &get_constants_of_type(TypePtr type) {
    return constants_of_type[type];
  }

  const auto &get_ground_predicates() const { return ground_predicates_; }

  // TODO use hashmap for faster indexing
  GroundPredicatePtr get_predicate_index(const GroundPredicate &predicate) {
    assert(ground_predicates_.count(predicate) > 0);
    return ground_predicates_[predicate];
  }

  const std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>> &
  get_predicate_support(bool is_negated, bool is_effect) {
    return select_support(is_negated, is_effect);
  }

  bool is_relevant(GroundPredicatePtr predicate) {
    // Predicate is in any precondition
    bool as_precondition = !select_support(true, false)[predicate].empty() ||
                           !select_support(false, false)[predicate].empty();
    if (as_precondition) {
      return true;
    }
    // Predicate is in goal
    return std::any_of(goal_.cbegin(), goal_.cend(),
                       [&predicate](const auto &goal_predicate) {
                         return predicate == goal_predicate.first;
                       });
  }

  bool is_rigid(GroundPredicatePtr predicate, bool negated) {
    // Is in initial state
    bool in_init = std::any_of(initial_state_.cbegin(), initial_state_.cend(),
                               [&predicate](const auto &initial_predicate) {
                                 return predicate == initial_predicate.first;
                               });
    if (in_init == negated) {
      return false;
    }
    // Opposite predicate is in any effect
    return select_support(!negated, true)[predicate].empty();
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
        number_arguments.push_back(constants_of_type[parameter.type].size());
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
        ground_predicates_.emplace(
            GroundPredicate{predicate_ptr, std::move(arguments)},
            ground_predicates_.size());
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

    auto &predicate_support = select_support(predicate.negated, is_effect);

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
      predicate_support[index].emplace_back(action_ptr, std::move(assignment));
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

  /* void set_predicate_support_alt(const Problem &problem) { */
  /*   auto pos_precondition_support = */
  /*       std::vector<std::vector<std::pair<ActionPtr, ArgumentMapping>>>( */
  /*           problem.predicates.size()); */
  /*   auto neg_precondition_support = */
  /*       std::vector<std::vector<std::pair<ActionPtr, ArgumentMapping>>>( */
  /*           problem.predicates.size()); */
  /*   auto pos_effect_support = */
  /*       std::vector<std::vector<std::pair<ActionPtr, ArgumentMapping>>>( */
  /*           problem.predicates.size()); */
  /*   auto neg_effect_support = */
  /*       std::vector<std::vector<std::pair<ActionPtr, ArgumentMapping>>>( */
  /*           problem.predicates.size()); */
  /*   for (ActionPtr action_ptr = 0; action_ptr < problem.actions.size(); */
  /*        ++action_ptr) { */
  /*     const auto &action = problem.actions[action_ptr]; */
  /*     for (const auto &predicate : action.preconditions) { */
  /*       auto &predicate_support = predicate.negated ?
   * neg_precondition_support */
  /*                                                   :
   * pos_precondition_support; */
  /*       predicate_support[predicate.definition].emplace_back( */
  /*           action_ptr, */
  /*           ArgumentMapping{action.parameters, predicate.arguments}); */
  /*     } */
  /*     for (const auto &predicate : action.effects) { */
  /*       auto &predicate_support = */
  /*           predicate.negated ? neg_effect_support : pos_effect_support; */
  /*       predicate_support[predicate.definition].emplace_back( */
  /*           action_ptr, */
  /*           ArgumentMapping{action.parameters, predicate.arguments}); */
  /*     } */
  /*   } */
  /*   pos_precondition_support_.resize(ground_predicates_.size()); */
  /*   neg_precondition_support_.resize(ground_predicates_.size()); */
  /*   pos_effect_support_.resize(ground_predicates_.size()); */
  /*   neg_effect_support_.resize(ground_predicates_.size()); */
  /*   for (GroundPredicatePtr predicate_ptr = 0; */
  /*        predicate_ptr < ground_predicates_.size(); ++predicate_ptr) { */
  /*     const auto & ground_predicate = ground_predicates_[predicate_ptr]; */
  /*     for (const auto &[action_ptr, mapping] : */
  /*          pos_precondition_support[ground_predicates_[predicate_ptr] */
  /*                                       .definition]) { */
  /*       const auto &action = problem.actions[action_ptr]; */
  /*       bool all_subtype = true; */
  /*       std::vector<size_t> arguments(mapping.size()); */
  /*       for (const auto &[parameter_pos, predicate_pos_list] : */
  /*            mapping.matches) { */
  /*         for (auto predicate_pos : predicate_pos_list) { */
  /*           if (!is_subtype( */
  /*                   problem.types, */
  /*                   problem.constants[ground_predicate.arguments[predicate_pos]]
   */
  /*                       .type, */
  /*                   action.parameters[parameter_pos].type)) { */
  /*             all_subtype = false; */
  /*             break; */
  /*           } */
  /*         } */
  /*         if (!all_subtype) { */
  /*           break; */
  /*         } */

  /*       } */
  /*       if (all_subtype) { */
  /*           for (size_t i = 0; i < mapping.size(); ++i) { */

  /*           } */
  /*           pos_precondition_support_[predicate_ptr].emplace_back(action_ptr,
   */
  /*       } */
  /*     } */
  /*   } */
  /* } */

  std::vector<std::pair<GroundPredicatePtr, bool>> initial_state_;
  std::vector<std::pair<GroundPredicatePtr, bool>> goal_;
  std::vector<std::vector<ConstantPtr>> constants_of_type;
  std::unordered_map<GroundPredicate, GroundPredicatePtr, GroundPredicateHash>
      ground_predicates_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>
      pos_precondition_support_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>
      neg_precondition_support_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>
      pos_effect_support_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>
      neg_effect_support_;
};

} // namespace support

using support::Support;

} // namespace model

#endif /* end of include guard: SUPPORT_HPP */
