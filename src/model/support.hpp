#ifndef SUPPORT_HPP
#define SUPPORT_HPP

#include "logging/logging.hpp"
#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "model/to_string.hpp"

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
                           const std::vector<Argument> &arguments) noexcept;

  inline size_t size() const noexcept { return matches.size(); }

  inline size_t get_parameter_index(size_t pos) const noexcept {
    assert(pos < matches.size());
    return matches[pos].first;
  }

  inline const auto &get_argument_matches(size_t pos) const noexcept {
    assert(pos < matches.size());
    return matches[pos].second;
  }

  std::vector<std::pair<size_t, std::vector<size_t>>> matches;
};

struct ArgumentAssignment {
  explicit ArgumentAssignment(const ArgumentMapping &mapping,
                              const std::vector<size_t> &arguments) noexcept;

  std::vector<std::pair<size_t, size_t>> arguments;
};

class Support {
public:
  explicit Support(Problem &problem_) noexcept;
  Support(const Support &support) = delete;
  Support &operator=(const Support &support) = delete;
  Support(Support &&support) = default;
  Support &operator=(Support &&support) = delete;

  void update() noexcept;

  inline const Problem &get_problem() const noexcept { return problem_; }

  inline size_t get_num_predicates() const noexcept {
    return problem_.predicates.size();
  }

  inline size_t get_num_actions() const noexcept {
    return problem_.actions.size();
  }

  inline const std::vector<ConstantPtr> &
  get_constants_of_type(TypePtr type) const noexcept {
    assert(type < constants_of_type_.size());
    return constants_of_type_[type];
  }

  inline const auto &get_ground_predicates(PredicatePtr predicate_ptr) const
      noexcept {
    assert(ground_predicates_constructed_);
    return ground_predicates_[predicate_ptr];
  }

  inline GroundPredicatePtr
  get_predicate_index(const GroundPredicate &predicate) const noexcept {
    assert(ground_predicates_constructed_ &&
           ground_predicates_[predicate.definition].count(predicate) > 0);
    return ground_predicates_[predicate.definition].at(predicate);
  }

  inline const std::vector<
      std::vector<std::pair<ActionPtr, ArgumentAssignment>>> &
  get_predicate_support(PredicatePtr predicate_ptr, bool is_negated,
                        bool is_effect) const noexcept {
    assert(predicate_support_constructed_);
    return select_support(predicate_ptr, is_negated, is_effect);
  }

  inline bool is_init(const GroundPredicate &predicate) const noexcept {
    return initial_state_.count(predicate) > 0;
  }

  bool is_rigid(const GroundPredicate &predicate, bool negated) const noexcept {
    assert(predicate_support_constructed_);
    auto index = get_predicate_index(predicate);
    return (negated
                ? neg_rigid_predicates_[predicate.definition].count(index)
                : pos_rigid_predicates_[predicate.definition].count(index)) ==
           1;
  }

  /* bool is_rigid(const GroundPredicate &predicate, bool negated) const
   * noexcept { */
  /*   assert(predicate_support_constructed_); */
  /*   // Is in initial state */
  /*   if (is_init(predicate) == negated) { */
  /*     return false; */
  /*   } */
  /*   // Opposite predicate is in any effect */
  /*   return select_support(predicate.definition, !negated, */
  /*                         true)[get_predicate_index(predicate)] */
  /*       .empty(); */
  /* } */

  inline const std::vector<std::pair<ActionPtr, ArgumentAssignment>> &
  get_forbidden_assignments() const noexcept {
    assert(predicate_support_constructed_);
    return forbidden_assignments_;
  }

  inline auto get_rigid(PredicatePtr predicate_ptr, bool negated) {
    return negated ? neg_rigid_predicates_[predicate_ptr]
                   : pos_rigid_predicates_[predicate_ptr];
  }

  template <typename Function>
  void for_grounded_predicate(const Action &action,
                              const PredicateEvaluation &predicate,
                              Function f) {
    support::ArgumentMapping mapping{action.parameters, predicate.arguments};

    std::vector<model::ConstantPtr> arguments(predicate.arguments.size());
    for (size_t i = 0; i < predicate.arguments.size(); ++i) {
      const auto &argument = predicate.arguments[i];
      if (auto constant = std::get_if<ConstantPtr>(&argument)) {
        arguments[i] = *constant;
      } else {
        auto parameter_ptr = std::get<ParameterPtr>(argument);
        if (action.parameters[parameter_ptr].constant) {
          arguments[i] = *action.parameters[parameter_ptr].constant;
        }
      }
    }

    model::GroundPredicate ground_predicate{predicate.definition,
                                            std::move(arguments)};
    std::vector<size_t> argument_sizes;
    argument_sizes.reserve(mapping.size());
    std::transform(
        mapping.matches.begin(), mapping.matches.end(),
        std::back_inserter(argument_sizes), [this, &action](const auto &m) {
          return get_constants_of_type(action.parameters[m.first].type).size();
        });

    auto combination_iterator = CombinationIterator{argument_sizes};

    while (!combination_iterator.end()) {
      const auto &combination = *combination_iterator;
      for (size_t i = 0; i < mapping.size(); ++i) {
        const auto &[parameter_pos, predicate_pos] = mapping.matches[i];
        auto type = action.parameters[parameter_pos].type;
        for (auto index : predicate_pos) {
          ground_predicate.arguments[index] =
              get_constants_of_type(type)[combination[i]];
        }
      }
      f(ground_predicate, ArgumentAssignment{mapping, combination});
      ++combination_iterator;
    }
  }

  bool simplify_action(Action &action) {
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

private:
  inline std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>> &
  select_support(PredicatePtr predicate_ptr, bool is_negated,
                 bool is_effect) noexcept {
    if (is_negated) {
      return is_effect ? neg_effect_support_[predicate_ptr]
                       : neg_precondition_support_[predicate_ptr];
    } else {
      return is_effect ? pos_effect_support_[predicate_ptr]
                       : pos_precondition_support_[predicate_ptr];
    }
  }

  inline const std::vector<
      std::vector<std::pair<ActionPtr, ArgumentAssignment>>> &
  select_support(PredicatePtr predicate_ptr, bool is_negated,
                 bool is_effect) const noexcept {
    if (is_negated) {
      return is_effect ? neg_effect_support_[predicate_ptr]
                       : neg_precondition_support_[predicate_ptr];
    } else {
      return is_effect ? pos_effect_support_[predicate_ptr]
                       : pos_precondition_support_[predicate_ptr];
    }
  }

  std::vector<std::vector<ConstantPtr>> sort_constants_by_type() noexcept;

  void ground_predicates() noexcept;

  void set_predicate_support() noexcept;

  std::unordered_set<GroundPredicate, GroundPredicateHash> initial_state_;
  std::vector<std::vector<ConstantPtr>> constants_of_type_;
  std::vector<std::unordered_map<GroundPredicate, GroundPredicatePtr,
                                 GroundPredicateHash>>
      ground_predicates_;
  std::vector<std::vector<std::vector<std::pair<ActionPtr, ArgumentMapping>>>>
      predicate_in_effect_;
  std::vector<
      std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>>
      pos_precondition_support_;
  std::vector<
      std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>>
      neg_precondition_support_;
  std::vector<
      std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>>
      pos_effect_support_;
  std::vector<
      std::vector<std::vector<std::pair<ActionPtr, ArgumentAssignment>>>>
      neg_effect_support_;
  std::vector<std::pair<ActionPtr, ArgumentAssignment>> forbidden_assignments_;
  std::vector<
      std::unordered_set<GroundPredicatePtr, IndexHash<GroundPredicate>>>
      pos_rigid_predicates_;
  std::vector<
      std::unordered_set<GroundPredicatePtr, IndexHash<GroundPredicate>>>
      neg_rigid_predicates_;
  bool ground_predicates_constructed_ = false;
  bool predicate_support_constructed_ = false;

  const Problem &problem_;
};

} // namespace support

using support::Support;

} // namespace model

#endif /* end of include guard: SUPPORT_HPP */
