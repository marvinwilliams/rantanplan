#include "support.hpp"
#include "logging/logging.hpp"
#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "util/combinatorics.hpp"

#include <algorithm>
#include <cassert>
#include <variant>

namespace support {

logging::Logger logger{"Support"};

using namespace model;

ArgumentMapping::ArgumentMapping(
    const std::vector<Parameter> &parameters,
    const std::vector<Argument> &arguments) noexcept {
  std::vector<std::vector<size_t>> parameter_matches{parameters.size()};
  for (size_t i = 0; i < arguments.size(); ++i) {
    auto parameter_ptr = std::get_if<ParameterPtr>(&arguments[i]);
    if (parameter_ptr && !parameters[*parameter_ptr].constant) {
      parameter_matches[*parameter_ptr].push_back(i);
    }
  }
  for (size_t i = 0; i < parameters.size(); ++i) {
    if (!parameter_matches[i].empty()) {
      matches.emplace_back(i, std::move(parameter_matches[i]));
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
            std::accumulate(
                ground_predicates_.begin(), ground_predicates_.end(), 0,
                [](size_t sum, const auto &p) { return sum + p.size(); }));
  LOG_DEBUG(logger, "Compute predicate support...");
  set_predicate_support();
  predicate_support_constructed_ = true;
  /* for (size_t i = 0; i < problem_.predicates.size(); ++i) { */
  /*   LOG_DEBUG(logger, "%s %s\n", */
  /*             model::to_string(problem_.predicates[i], problem_).c_str(), */
  /*             is_rigid(i) ? "rigid" : "not rigid"); */
  /* } */
  LOG_DEBUG(logger, "");
  if constexpr (logging::has_debug_log()) {
    for (size_t i = 0; i < get_num_predicates(); ++i) {
      for (const auto &predicate : ground_predicates_[i]) {
        LOG_DEBUG(logger, "%s %s\n",
                  model::to_string(predicate.first, problem_).c_str(),
                  is_rigid(predicate.first, false) ? "rigid" : "not rigid");
        LOG_DEBUG(logger, "!%s %s\n",
                  model::to_string(predicate.first, problem_).c_str(),
                  is_rigid(predicate.first, true) ? "rigid" : "not rigid");
      }
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
  pos_rigid_predicates_.clear();
  neg_rigid_predicates_.clear();
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
  ground_predicates_.resize(problem_.predicates.size());
  for (PredicatePtr predicate_ptr = 0;
       predicate_ptr < problem_.predicates.size(); ++predicate_ptr) {
    const PredicateDefinition &predicate = problem_.predicates[predicate_ptr];

    std::vector<size_t> number_arguments;
    number_arguments.reserve(predicate.parameters.size());
    for (const auto &parameter : predicate.parameters) {
      number_arguments.push_back(constants_of_type_[parameter.type].size());
    }

    auto combination_iterator = CombinationIterator{number_arguments};

    ground_predicates_[predicate_ptr].reserve(
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
      ground_predicates_[predicate_ptr].emplace(
          GroundPredicate{predicate_ptr, std::move(arguments)},
          ground_predicates_[predicate_ptr].size());
      ++combination_iterator;
    }
  }
}

void Support::set_predicate_support() noexcept {
  /* bool is_effect; */
  pos_precondition_support_.resize(problem_.predicates.size());
  neg_precondition_support_.resize(problem_.predicates.size());
  pos_effect_support_.resize(problem_.predicates.size());
  neg_effect_support_.resize(problem_.predicates.size());
  for (size_t i = 0; i < ground_predicates_.size(); ++i) {
    pos_precondition_support_[i].resize(ground_predicates_[i].size());
    neg_precondition_support_[i].resize(ground_predicates_[i].size());
    pos_effect_support_[i].resize(ground_predicates_[i].size());
    neg_effect_support_[i].resize(ground_predicates_[i].size());
  }
  for (ActionPtr action_ptr = 0; action_ptr < problem_.actions.size();
       ++action_ptr) {
    const auto &action = problem_.actions[action_ptr];
    for (const auto &predicate : action.preconditions) {
      for_grounded_predicate(
          action, predicate,
          [this, action_ptr,
           &predicate](const GroundPredicate &ground_predicate,
                       ArgumentAssignment assignment) {
            auto &predicate_support =
                select_support(predicate.definition, predicate.negated, false);
            auto index = get_predicate_index(ground_predicate);
            predicate_support[index].emplace_back(action_ptr,
                                                  std::move(assignment));
          });
    }
    for (const auto &predicate : problem_.actions[action_ptr].effects) {
      /* assert(!is_rigid(predicate.definition)); */
      for_grounded_predicate(
          action, predicate,
          [this, action_ptr,
           &predicate](const GroundPredicate &ground_predicate,
                       ArgumentAssignment assignment) {
            auto &predicate_support =
                select_support(predicate.definition, predicate.negated, true);
            auto index = get_predicate_index(ground_predicate);
            predicate_support[index].emplace_back(action_ptr,
                                                  std::move(assignment));
          });
    }
  }
  pos_rigid_predicates_.resize(problem_.predicates.size());
  neg_rigid_predicates_.resize(problem_.predicates.size());
  for (PredicatePtr predicate_ptr = 0;
       predicate_ptr < problem_.predicates.size(); ++predicate_ptr) {
    for (const auto &[ground_predicate, ground_predicate_ptr] :
         ground_predicates_[predicate_ptr]) {
      if (neg_effect_support_[predicate_ptr][ground_predicate_ptr].empty() &&
          is_init(ground_predicate)) {
        pos_rigid_predicates_[predicate_ptr].insert(ground_predicate_ptr);
      }
      if (pos_effect_support_[predicate_ptr][ground_predicate_ptr].empty() &&
          !is_init(ground_predicate)) {
        neg_rigid_predicates_[predicate_ptr].insert(ground_predicate_ptr);
      }
    }
  }
}

bool Support::simplify_action(model::Action &action) noexcept {
  bool valid = true;
  auto end =
      std::remove_if(action.preconditions.begin(), action.preconditions.end(),
                     [&](const auto &p) {
                       if (is_grounded(p, action)) {
                         model::GroundPredicate ground_predicate{p, action};
                         if (is_rigid(ground_predicate, p.negated)) {
                           return true;
                         } else if (is_rigid(ground_predicate, !p.negated)) {
                           valid = false;
                         }
                       }
                       return false;
                     });
  if (!valid) {
    return false;
  }
  action.preconditions.erase(end, action.preconditions.end());
  return true;
}

} // namespace support
