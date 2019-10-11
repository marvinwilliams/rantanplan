#ifndef SUPPORT_HPP
#define SUPPORT_HPP

#include "logging/logging.hpp"
#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "model/to_string.hpp"

#include <cassert>
#include <numeric>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace model {

namespace support {

extern logging::Logger logger;

struct ArgumentMapping {
  explicit ArgumentMapping(const std::vector<Parameter> &parameters,
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

  inline size_t size() const noexcept { return matches.size(); }

  inline size_t get_parameter_index(size_t pos) const noexcept {
    assert(pos < matches.size());
    return matches[pos].first;
  }

  inline const std::vector<size_t> &get_argument_matches(size_t pos) const
      noexcept {
    assert(pos < matches.size());
    return matches[pos].second;
  }

  std::vector<std::pair<size_t, std::vector<size_t>>> matches;
};

struct ArgumentAssignment {
  explicit ArgumentAssignment(const ArgumentMapping &mapping,
                              const std::vector<size_t> &arguments) noexcept {
    assert(arguments.size() == mapping.size());
    this->arguments.reserve(mapping.size());
    for (size_t i = 0; i < mapping.size(); ++i) {
      this->arguments.emplace_back(mapping.get_parameter_index(i),
                                   arguments[i]);
    }
  }

  inline const std::vector<std::pair<size_t, size_t>> &get_arguments() const
      noexcept {
    return arguments;
  }

  std::vector<std::pair<size_t, size_t>> arguments;
};

class Support {
public:
  explicit Support(Problem &problem_) noexcept : problem_{problem_} {
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

  void update() noexcept {
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

  const Problem &get_problem() const noexcept { return problem_; }

  inline const std::vector<ConstantPtr> &
  get_constants_of_type(TypePtr type) const noexcept {
    assert(type < constants_of_type_.size());
    return constants_of_type_[type];
  }

  inline const auto &get_ground_predicates() const noexcept {
    assert(ground_predicates_constructed_);
    return ground_predicates_;
  }

  inline GroundPredicatePtr
  get_predicate_index(const GroundPredicate &predicate) noexcept {
    assert(ground_predicates_constructed_ &&
           ground_predicates_.count(predicate) > 0);
    return ground_predicates_[predicate];
  }

  inline const std::vector<
      std::vector<std::pair<ActionPtr, ArgumentAssignment>>> &
  get_predicate_support(bool is_negated, bool is_effect) noexcept {
    assert(predicate_support_constructed_);
    return select_support(is_negated, is_effect);
  }

  inline bool is_rigid(PredicatePtr predicate_ptr) const noexcept {
    assert(predicate_ptr < predicate_in_effect_.size());
    return predicate_in_effect_[predicate_ptr].empty();
  }

  /* bool is_relevant(const GroundPredicate& predicate) { */
  /*   if (is_rigid(predicate.definition)) { */
  /*     return true; */
  /*   } */
  /*   // Predicate is in any precondition */
  /*   bool as_precondition = !select_support(true, false)[predicate].empty() ||
   */
  /*                          !select_support(false, false)[predicate].empty();
   */
  /*   if (as_precondition) { */
  /*     return true; */
  /*   } */
  /*   // Predicate is in goal */
  /*   return std::any_of(goal_.cbegin(), goal_.cend(), */
  /*                      [&predicate](const auto &goal_predicate) { */
  /*                        return predicate == goal_predicate.first; */
  /*                      }); */
  /* } */

  inline bool is_init(const GroundPredicate &predicate) const noexcept {
    return initial_state_.count(predicate) > 0;
  }

  bool is_rigid(const GroundPredicate &predicate, bool negated) noexcept {
    assert(predicate_support_constructed_);
    // Is in initial state
    if (is_init(predicate) == negated) {
      return false;
    }
    if (is_rigid(predicate.definition)) {
      return true;
    }
    // Opposite predicate is in any effect
    return select_support(!negated, true)[get_predicate_index(predicate)]
        .empty();
  }

  inline const std::vector<std::pair<ActionPtr, ArgumentAssignment>> &
  get_forbidden_assignments() noexcept {
    assert(predicate_support_constructed_);
    return forbidden_assignments_;
  }

  bool is_grounded(const model::PredicateEvaluation &predicate) const noexcept {
    return std::all_of(predicate.arguments.begin(), predicate.arguments.end(),
                       [](const auto &a) {
                         return std::holds_alternative<model::ConstantPtr>(a);
                       });
  }

private:
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>> &
  select_support(bool is_negated, bool is_effect) noexcept {
    if (is_negated) {
      return is_effect ? neg_effect_support_ : neg_precondition_support_;
    } else {
      return is_effect ? pos_effect_support_ : pos_precondition_support_;
    }
  }

  std::vector<std::vector<ConstantPtr>> sort_constants_by_type() noexcept {
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

  void ground_predicates() noexcept {
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

      auto combinations = all_combinations(number_arguments);

      ground_predicates_.reserve(ground_predicates_.size() +
                                 combinations.size());
      for (const auto &combination : combinations) {
        std::vector<ConstantPtr> arguments;
        arguments.reserve(combination.size());
        for (size_t j = 0; j < combination.size(); ++j) {
          auto type = predicate.parameters[j].type;
          auto constant = constants_of_type_[type][combination[j]];
          arguments.push_back(constant);
        }
        ground_predicates_.emplace(
            GroundPredicate{predicate_ptr, std::move(arguments)},
            ground_predicates_.size());
      }
    }
  }

  void ground_action_predicate(const Problem &problem_, ActionPtr action_ptr,
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

    auto combinations{all_combinations(number_arguments)};

    for (const auto &combination : combinations) {
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
          forbidden_assignments_.emplace_back(action_ptr,
                                              std::move(assignment));
        }
      } else {
        auto &predicate_support = select_support(predicate.negated, is_effect);
        auto index = get_predicate_index(ground_predicate);
        predicate_support[index].emplace_back(action_ptr,
                                              std::move(assignment));
      }
    }
  }

  void set_predicate_support() noexcept {
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

  /* void set_predicate_support_alt(const Problem &problem_) { */
  /*   auto pos_precondition_support = */
  /*       std::vector<std::vector<std::pair<ActionPtr, ArgumentMapping>>>( */
  /*           problem_.predicates.size()); */
  /*   auto neg_precondition_support = */
  /*       std::vector<std::vector<std::pair<ActionPtr, ArgumentMapping>>>( */
  /*           problem_.predicates.size()); */
  /*   auto pos_effect_support = */
  /*       std::vector<std::vector<std::pair<ActionPtr, ArgumentMapping>>>( */
  /*           problem_.predicates.size()); */
  /*   auto neg_effect_support = */
  /*       std::vector<std::vector<std::pair<ActionPtr, ArgumentMapping>>>( */
  /*           problem_.predicates.size()); */
  /*   for (ActionPtr action_ptr = 0; action_ptr < problem_.actions.size(); */
  /*        ++action_ptr) { */
  /*     const auto &action = problem_.actions[action_ptr]; */
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
  /*       const auto &action = problem_.actions[action_ptr]; */
  /*       bool all_subtype = true; */
  /*       std::vector<size_t> arguments(mapping.size()); */
  /*       for (const auto &[parameter_pos, predicate_pos_list] : */
  /*            mapping.matches) { */
  /*         for (auto predicate_pos : predicate_pos_list) { */
  /*           if (!is_subtype( */
  /*                   problem_.types, */
  /*                   problem_.constants[ground_predicate.arguments[predicate_pos]]
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

  std::unordered_set<GroundPredicate, GroundPredicateHash> initial_state_;
  std::vector<std::vector<ConstantPtr>> constants_of_type_;
  std::unordered_map<GroundPredicate, GroundPredicatePtr, GroundPredicateHash>
      ground_predicates_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentMapping>>>
      predicate_in_effect_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>
      pos_precondition_support_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>
      neg_precondition_support_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>
      pos_effect_support_;
  std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>
      neg_effect_support_;
  std::vector<std::pair<ActionPtr, ArgumentAssignment>> forbidden_assignments_;
  bool ground_predicates_constructed_ = false;
  bool predicate_support_constructed_ = false;

  const Problem &problem_;
};

} // namespace support

using support::Support;

} // namespace model

#endif /* end of include guard: SUPPORT_HPP */
