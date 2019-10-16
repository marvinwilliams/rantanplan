#include "support.hpp"
#include "logging/logging.hpp"
#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "util/combinatorics.hpp"

#include <cassert>

namespace model {

namespace support {

logging::Logger logger{"Support"};

ArgumentMapping::ArgumentMapping(
    const std::vector<Parameter> &parameters,
    const std::vector<Argument> &arguments) noexcept {
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

ArgumentAssignment::ArgumentAssignment(
    const ArgumentMapping &mapping,
    const std::vector<size_t> &arguments) noexcept {
  assert(arguments.size() == mapping.size());
  this->arguments.reserve(mapping.size());
  for (size_t i = 0; i < mapping.size(); ++i) {
    this->arguments.emplace_back(mapping.get_parameter_index(i), arguments[i]);
  }
}

Support::Support(Problem &problem_) noexcept : problem_{problem_} {
  initial_state_.reserve(problem_.initial_state.size());
  for (const PredicateEvaluation &predicate : problem_.initial_state) {
    assert(!predicate.negated);
    initial_state_.emplace(predicate);
  }
  LOG_DEBUG(logger, "Sort constants by type...");
  constants_of_type_ = sort_constants_by_type();
  LOG_DEBUG(logger, "Ground all predicates...");
  ground_predicates();
  ground_predicates_constructed_ = true;
  LOG_DEBUG(logger, "The problem_ has %u grounded predicates",
            ground_predicates_.size());
  LOG_DEBUG(logger, "Compute predicate support...");
  set_predicate_support();
  predicate_support_constructed_ = true;
  for (size_t i = 0; i < problem_.predicates.size(); ++i) {
    LOG_DEBUG(logger, "%s %s\n",
              model::to_string(problem_.predicates[i], problem_).c_str(),
              is_rigid(i) ? "rigid" : "not rigid");
  }
  LOG_DEBUG(logger, "");
  if constexpr (logging::has_debug_log()) {
    for (const auto &predicate : ground_predicates_) {
      LOG_DEBUG(logger, "%s %s\n",
                model::to_string(predicate.first, problem_).c_str(),
                is_rigid(predicate.first, false) ? "rigid" : "not rigid");
      LOG_DEBUG(logger, "!%s %s\n",
                model::to_string(predicate.first, problem_).c_str(),
                is_rigid(predicate.first, true) ? "rigid" : "not rigid");
    }
  }
}

void Support::update() noexcept {
  predicate_in_effect_.clear();
  ground_predicates_.clear();
  pos_precondition_support_.clear();
  neg_precondition_support_.clear();
  pos_effect_support_.clear();
  neg_effect_support_.clear();
  forbidden_assignments_.clear();
  ground_predicates();
  set_predicate_support();
}

std::vector<std::vector<ConstantPtr>>
Support::sort_constants_by_type() noexcept {
  std::vector<std::vector<ConstantPtr>> constants_of_type(
      problem_.types.size());
  for (ConstantPtr constant_ptr = 0; constant_ptr < problem_.constants.size();
       ++constant_ptr) {
    TypePtr type = problem_.constants[constant_ptr].type;
    constants_of_type[type].push_back(constant_ptr);
    while (problem_.types[type].parent != type) {
      type = problem_.types[type].parent;
      constants_of_type[type].push_back(constant_ptr);
    }
  }
  return constants_of_type;
}

void Support::ground_predicates() noexcept {
  predicate_in_effect_.resize(problem_.predicates.size());
  for (ActionPtr action_ptr = 0; action_ptr < problem_.actions.size();
       ++action_ptr) {
    const auto &action = problem_.actions[action_ptr];
    for (const auto &effect : action.effects) {
      predicate_in_effect_[effect.definition].emplace_back(
          action_ptr, ArgumentMapping{action.parameters, effect.arguments});
    }
  }
  for (PredicatePtr predicate_ptr = 0;
       predicate_ptr < problem_.predicates.size(); ++predicate_ptr) {
    if (is_rigid(predicate_ptr)) {
      continue;
    }
    const PredicateDefinition &predicate = problem_.predicates[predicate_ptr];

    std::vector<size_t> number_arguments;
    number_arguments.reserve(predicate.parameters.size());
    for (const auto &parameter : predicate.parameters) {
      number_arguments.push_back(constants_of_type_[parameter.type].size());
    }

    auto combination_iterator = CombinationIterator{number_arguments};

    ground_predicates_.reserve(ground_predicates_.size() +
                               combination_iterator.number_combinations());
    while (!combination_iterator.end()) {
      const auto &combination = *combination_iterator;
      std::vector<ConstantPtr> arguments;
      arguments.reserve(combination.size());
      for (size_t i = 0; i < combination.size(); ++i) {
        auto type = predicate.parameters[i].type;
        auto constant = constants_of_type_[type][combination[i]];
        arguments.push_back(constant);
      }
      ground_predicates_.emplace(
          GroundPredicate{predicate_ptr, std::move(arguments)},
          ground_predicates_.size());
      ++combination_iterator;
    }
  }
}

void Support::ground_action_predicate(const Problem &problem_,
                                      ActionPtr action_ptr,
                                      const PredicateEvaluation &predicate,
                                      bool is_effect) noexcept {
  const auto &action = problem_.actions[action_ptr];
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
    number_arguments.push_back(constants_of_type_[type].size());
  }

  auto combination_iterator = CombinationIterator{number_arguments};

  while (!combination_iterator.end()) {
      const auto &combination = *combination_iterator;
    for (size_t i = 0; i < mapping.size(); ++i) {
      auto type = action.parameters[mapping.get_parameter_index(i)].type;
      for (auto index : mapping.get_argument_matches(i)) {
        ground_predicate.arguments[index] =
            get_constants_of_type(type)[combination[i]];
      }
    }
    ArgumentAssignment assignment{mapping, combination};
    if (is_rigid(predicate.definition)) {
      if (is_init(ground_predicate) == predicate.negated) {
        // This predicate is never satisfiable
        LOG_DEBUG(logger, "Forbid assignment for %s :%s %s\n",
                  model::to_string(action, problem_).c_str(),
                  predicate.negated ? "!" : "",
                  model::to_string(ground_predicate, problem_).c_str());
        forbidden_assignments_.emplace_back(action_ptr, std::move(assignment));
      }
    } else {
      auto &predicate_support = select_support(predicate.negated, is_effect);
      auto index = get_predicate_index(ground_predicate);
      predicate_support[index].emplace_back(action_ptr, std::move(assignment));
    }
    ++combination_iterator;
  }
}

void Support::set_predicate_support() noexcept {
  pos_precondition_support_.resize(ground_predicates_.size());
  neg_precondition_support_.resize(ground_predicates_.size());
  pos_effect_support_.resize(ground_predicates_.size());
  neg_effect_support_.resize(ground_predicates_.size());
  for (ActionPtr action_ptr = 0; action_ptr < problem_.actions.size();
       ++action_ptr) {
    for (const auto &predicate : problem_.actions[action_ptr].preconditions) {
      ground_action_predicate(problem_, action_ptr, predicate, false);
    }
    for (const auto &predicate : problem_.actions[action_ptr].effects) {
      assert(!is_rigid(predicate.definition));
      ground_action_predicate(problem_, action_ptr, predicate, true);
    }
  }
}

} // namespace support

} // namespace model
