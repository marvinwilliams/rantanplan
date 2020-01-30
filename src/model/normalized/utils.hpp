#ifndef NORMALIZED_UTILS_HPP
#define NORMALIZED_UTILS_HPP

#include "model/normalized/model.hpp"
#include "util/combination_iterator.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <iterator>
#include <numeric>
#include <variant>
#include <vector>

namespace normalized {

inline bool is_subtype(TypeIndex subtype, TypeIndex type,
                       const Problem &problem) noexcept {
  if (subtype == type) {
    return true;
  }
  while (problem.types[subtype].supertype != subtype) {
    subtype = problem.types[subtype].supertype;
    if (subtype == type) {
      return true;
    }
  }
  return false;
}

using ParameterSelection = std::vector<ParameterIndex>;

struct ParameterMapping {
  ParameterSelection parameters;
  std::vector<std::vector<ArgumentIndex>> arguments;
};

using ParameterAssignment =
    std::vector<std::pair<ParameterIndex, ConstantIndex>>;

inline PredicateInstantiation instantiate(const Condition &condition) noexcept {
  std::vector<ConstantIndex> args;
  args.reserve(condition.arguments.size());
  for (const auto &arg : condition.arguments) {
    assert(arg.is_constant());
    args.push_back(arg.get_constant());
  }
  return PredicateInstantiation{condition.definition, std::move(args)};
}

inline PredicateInstantiation instantiate(const Condition &predicate,
                                          const Action &action) noexcept {
  std::vector<ConstantIndex> args;
  args.reserve(predicate.arguments.size());
  for (const auto &arg : predicate.arguments) {
    if (arg.is_constant()) {
      args.push_back(arg.get_constant());
    } else {
      assert(action.get(arg.get_parameter()).is_constant());
      args.push_back(action.get(arg.get_parameter()).get_constant());
    }
  }
  return PredicateInstantiation{predicate.definition, std::move(args)};
}

// returns true if every argument is constant
inline bool update_arguments(Condition &condition, const Action &action) {
  bool is_grounded = true;
  for (auto &arg : condition.arguments) {
    if (!arg.is_constant()) {
      if (action.parameters[arg.get_parameter()].is_constant()) {
        arg.set(action.get(arg.get_parameter()).get_constant());
      } else {
        is_grounded = false;
      }
    }
  }
  return is_grounded;
}

inline bool is_grounded(const Condition &predicate) noexcept {
  return std::all_of(predicate.arguments.cbegin(), predicate.arguments.cend(),
                     [](const auto &arg) { return arg.is_constant(); });
}

inline Action ground(const ParameterAssignment &assignment,
                     const Action &action) noexcept {
  Action new_action;
  new_action.parameters = action.parameters;
  for (auto [p, c] : assignment) {
    new_action.parameters[p].set(c);
  }
  new_action.pre_instantiated = action.pre_instantiated;
  new_action.eff_instantiated = action.eff_instantiated;

  for (auto pred : action.preconditions) {
    if (bool is_grounded = update_arguments(pred, new_action); is_grounded) {
      new_action.pre_instantiated.emplace_back(instantiate(pred),
                                               pred.positive);
    } else {
      new_action.preconditions.push_back(std::move(pred));
    }
  }
  for (auto pred : action.effects) {
    if (bool is_grounded = update_arguments(pred, new_action); is_grounded) {
      new_action.eff_instantiated.emplace_back(instantiate(pred),
                                               pred.positive);
    } else {
      new_action.effects.push_back(std::move(pred));
    }
  }
  return new_action;
}

inline ParameterMapping get_mapping(const Action &action,
                                    const Condition &predicate) noexcept {
  std::vector<std::vector<ArgumentIndex>> parameter_matches(
      action.parameters.size());
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    if (!predicate.arguments[i].is_constant()) {
      parameter_matches[predicate.arguments[i].get_parameter()].emplace_back(i);
    }
  }

  ParameterMapping mapping;
  for (size_t i = 0; i < action.parameters.size(); ++i) {
    if (!parameter_matches[i].empty()) {
      mapping.parameters.emplace_back(i);
      mapping.arguments.push_back(std::move(parameter_matches[i]));
    }
  }
  return mapping;
}

inline ParameterSelection
get_referenced_parameters(const Condition &predicate,
                          const Action &action) noexcept {
  std::vector<bool> parameter_matches(action.parameters.size(), false);
  for (size_t i = 0; i < predicate.arguments.size(); ++i) {
    if (!predicate.arguments[i].is_constant()) {
      parameter_matches[predicate.arguments[i].get_parameter()] = true;
    }
  }

  ParameterSelection selection;
  for (size_t i = 0; i < action.parameters.size(); ++i) {
    if (parameter_matches[i]) {
      selection.emplace_back(i);
    }
  }
  return selection;
}

inline ParameterAssignment
get_assignment(const ParameterMapping &mapping,
               const std::vector<ConstantIndex> &arguments) noexcept {
  assert(mapping.parameters.size() == arguments.size());
  ParameterAssignment assignment;
  assignment.reserve(mapping.parameters.size());
  for (size_t i = 0; i < mapping.parameters.size(); ++i) {
    assignment.push_back({mapping.parameters[i], arguments[i]});
  }
  return assignment;
}

inline size_t get_num_instantiated(const Predicate &predicate,
                                   const Problem &problem) {

  return std::accumulate(
      predicate.parameter_types.begin(), predicate.parameter_types.end(), 1ul,
      [&problem](size_t product, TypeIndex type) {
        return product * problem.constants_by_type[type].size();
      });
}

inline size_t get_num_instantiated(const Action &action,
                                   const Problem &problem) noexcept {
  return std::accumulate(
      action.parameters.begin(), action.parameters.end(), 1ul,
      [&problem](size_t product, const Parameter &p) {
        if (p.is_constant()) {
          return product;
        }
        return product * problem.constants_by_type[p.get_type()].size();
      });
}

inline size_t get_num_instantiated(const ParameterSelection &selection,
                                   const Action &action,
                                   const Problem &problem) noexcept {
  return std::accumulate(selection.begin(), selection.end(), 1ul,
                         [&action, &problem](size_t product, auto index) {
                           auto type = action.get(index).get_type();
                           return product *
                                  problem.constants_by_type[type].size();
                         });
}

class AssignmentIterator {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = ParameterAssignment;
  using difference_type = std::ptrdiff_t;
  using pointer = const value_type *;
  using reference = const value_type &;

  explicit AssignmentIterator() noexcept = default;

  explicit AssignmentIterator(const ParameterSelection &selection,
                              const Action &action, const Problem &problem)
      : selection_{&selection}, action_{&action}, problem_{&problem} {
    std::vector<size_t> argument_size_list;
    argument_size_list.reserve(selection.size());
    for (auto p : selection) {
      assert(!action.get(p).is_constant());
      argument_size_list.push_back(
          problem.constants_by_type[action.get(p).get_type()].size());
    }

    combination_iterator_ = util::CombinationIterator{argument_size_list};
    assignment_.resize(selection.size());

    if (combination_iterator_ != util::CombinationIterator{}) {
      const auto &combination = *combination_iterator_;
      for (size_t i = 0; i < selection.size(); ++i) {
        assert(!action.get(selection[i]).is_constant());
        auto type = action_->get(selection[i]).get_type();
        assignment_[i] = std::make_pair(
            (*selection_)[i], problem.constants_by_type[type][combination[i]]);
      }
    }
  }

  AssignmentIterator &operator++() noexcept {
    assert(problem_);
    ++combination_iterator_;
    if (combination_iterator_ != util::CombinationIterator{}) {
      const auto &combination = *combination_iterator_;
      for (size_t i = 0; i < combination.size(); ++i) {
        assert(!action_->get((*selection_)[i]).is_constant());
        auto type = action_->get((*selection_)[i]).get_type();
        assignment_[i] =
            std::make_pair((*selection_)[i],
                           problem_->constants_by_type[type][combination[i]]);
      }
    }
    return *this;
  }

  AssignmentIterator operator++(int) noexcept {
    auto old = *this;
    ++(*this);
    return old;
  }

  size_t get_num_instantiations() const noexcept {
    return combination_iterator_.get_num_combinations();
  }

  inline reference operator*() const noexcept { return assignment_; }

  bool operator!=(const AssignmentIterator &) const noexcept {
    return combination_iterator_ != util::CombinationIterator{};
  }

  bool operator==(const AssignmentIterator &) const noexcept {
    return combination_iterator_ == util::CombinationIterator{};
  }

private:
  ParameterAssignment assignment_;
  util::CombinationIterator combination_iterator_;
  const ParameterSelection *selection_ = nullptr;
  const Action *action_ = nullptr;
  const Problem *problem_ = nullptr;
};

class ConditionIterator {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = PredicateInstantiation;
  using difference_type = std::ptrdiff_t;
  using pointer = const value_type *;
  using reference = const value_type &;

  explicit ConditionIterator() noexcept : predicate_{PredicateIndex{0}, {}} {}

  explicit ConditionIterator(const Condition &condition, const Action &action,
                             const Problem &problem)
      : predicate_{condition.definition,
                   std::vector<ConstantIndex>(condition.arguments.size())},
        mapping_{get_mapping(action, condition)}, assignment_iterator_{
                                                      mapping_.parameters,
                                                      action, problem} {

    for (size_t i = 0; i < condition.arguments.size(); ++i) {
      if (condition.arguments[i].is_constant()) {
        predicate_.arguments[i] = condition.arguments[i].get_constant();
      }
    }
    if (assignment_iterator_ != AssignmentIterator{}) {
      const auto &assignment = *assignment_iterator_;
      for (size_t i = 0; i < mapping_.parameters.size(); ++i) {
        assert(assignment[i].first == mapping_.parameters[i]);
        for (auto a : mapping_.arguments[i]) {
          predicate_.arguments[a] = assignment[i].second;
        }
      }
    }
  }

  ConditionIterator &operator++() noexcept {
    ++assignment_iterator_;
    if (assignment_iterator_ != AssignmentIterator{}) {
      const auto &assignment = *assignment_iterator_;
      for (size_t i = 0; i < mapping_.parameters.size(); ++i) {
        for (auto a : mapping_.arguments[i]) {
          predicate_.arguments[a] = assignment[i].second;
        }
      }
    }
    return *this;
  }

  ConditionIterator operator++(int) noexcept {
    auto old = *this;
    ++(*this);
    return old;
  }

  size_t get_num_instantiations() const noexcept {
    return assignment_iterator_.get_num_instantiations();
  }

  inline reference operator*() const noexcept { return predicate_; }

  const auto &get_assignment() const noexcept { return *assignment_iterator_; }

  bool operator!=(const ConditionIterator &) const noexcept {
    return assignment_iterator_ != AssignmentIterator{};
  }

  bool operator==(const ConditionIterator &) const noexcept {
    return assignment_iterator_ == AssignmentIterator{};
  }

private:
  PredicateInstantiation predicate_;
  ParameterMapping mapping_;
  AssignmentIterator assignment_iterator_;
};

inline bool is_instantiatable(const Condition &condition,
                              const std::vector<ConstantIndex> &arguments,
                              const Action &action,
                              const Problem &problem) noexcept {
  auto parameters = action.parameters;
  for (size_t i = 0; i < condition.arguments.size(); ++i) {
    if (condition.arguments[i].is_constant()) {
      if (condition.arguments[i].get_constant() != arguments[i]) {
        return false;
      }
    } else {
      auto &p = parameters[condition.arguments[i].get_parameter()];
      if (p.is_constant()) {
        if (p.get_constant() != arguments[i]) {
          return false;
        }
      } else {
        if (!is_subtype(problem.get(arguments[i]).type, p.get_type(),
                        problem)) {
          return false;
        }
        p.set(arguments[i]);
      }
    }
  }
  return true;
}

inline bool is_unifiable(const Condition &first_condition,
                         const Action &first_action,
                         const Condition &second_condition,
                         const Action &second_action,
                         const Problem &problem) noexcept {
  assert(first_condition.definition == second_condition.definition);
  auto first_parameters = first_action.parameters;
  auto second_parameters = second_action.parameters;
  for (size_t i = 0; i < first_condition.arguments.size(); ++i) {
    const auto &first_p = first_condition.arguments[i];
    const auto &second_p = second_condition.arguments[i];
    if (first_p.is_constant() && second_p.is_constant()) {
      if (first_p.get_constant() != second_p.get_constant()) {
        return false;
      }
    } else if (second_p.is_constant()) {
      auto &action_p =
          first_parameters[first_condition.arguments[i].get_parameter()];
      if (action_p.is_constant()) {
        if (action_p.get_constant() != second_p.get_constant()) {
          return false;
        }
      } else {
        if (!is_subtype(problem.get(second_p.get_constant()).type,
                        action_p.get_type(), problem)) {
          return false;
        }
        action_p.set(second_p.get_constant());
      }
    } else if (first_p.is_constant()) {
      auto &action_p =
          second_parameters[second_condition.arguments[i].get_parameter()];
      if (action_p.is_constant()) {
        if (action_p.get_constant() != first_p.get_constant()) {
          return false;
        }
      } else {
        if (!is_subtype(problem.get(first_p.get_constant()).type,
                        action_p.get_type(), problem)) {
          return false;
        }
        action_p.set(first_p.get_constant());
      }
    }
  }
  return true;
}

} // namespace normalized

#endif /* end of include guard: NORMALIZED_UTILS_HPP */
