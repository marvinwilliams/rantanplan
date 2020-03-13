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

inline bool is_subtype(Type subtype, Type type,
                       const Problem &problem) noexcept {
  if (subtype.id == type.id) {
    return true;
  }
  while (subtype.supertype != subtype.id) {
    subtype = problem.types[subtype.supertype];
    if (subtype.id == type.id) {
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

using ParameterAssignment = std::vector<std::pair<ParameterIndex, Constant>>;

inline GroundAtom as_ground_atom(const Atom &atom) noexcept {
  GroundAtom ground_atom;
  ground_atom.predicate = atom.predicate;
  ground_atom.arguments.reserve(atom.arguments.size());
  for (const Argument &a : atom.arguments) {
    assert(!a.is_parameter());
    ground_atom.arguments.push_back(a.get_constant());
  }
  return ground_atom;
}

inline GroundAtom as_ground_atom(const Atom &atom,
                                 const Action &action) noexcept {
  GroundAtom ground_atom;
  ground_atom.predicate = atom.predicate;
  ground_atom.arguments.reserve(atom.arguments.size());
  for (const Argument &a : atom.arguments) {
    if (a.is_parameter()) {
      assert(!action.parameters[a.get_parameter_index()].is_free());
      ground_atom.arguments.push_back(
          action.parameters[a.get_parameter_index()].get_constant());
    } else {
      ground_atom.arguments.push_back(a.get_constant());
    }
  }
  return ground_atom;
}

// returns true if every argument is constant
inline void update_condition(Condition &condition, const Action &action) {
  for (Argument &a : condition.atom.arguments) {
    if (a.is_parameter() &&
        !action.parameters[a.get_parameter_index()].is_free()) {
      a.set(action.parameters[a.get_parameter_index()].get_constant());
    }
  }
}

inline bool is_ground(const Atom &atom) noexcept {
  return std::all_of(atom.arguments.cbegin(), atom.arguments.cend(),
                     [](const Argument &a) { return !a.is_parameter(); });
}

inline void ground(const ParameterAssignment &assignment,
                   Action &action) noexcept {
  for (auto [p, c] : assignment) {
    action.parameters[p].set(c);
  }

  for (Condition &c : action.preconditions) {
    update_condition(c, action);
  }

  for (Condition &c : action.effects) {
    update_condition(c, action);
  }
}

inline ParameterMapping get_mapping(const Atom &atom,
                                    const Action &action) noexcept {
  std::vector<std::vector<ArgumentIndex>> parameter_matches(
      action.parameters.size());

  for (size_t i = 0; i < atom.arguments.size(); ++i) {
    if (atom.arguments[i].is_parameter()) {
      parameter_matches[atom.arguments[i].get_parameter_index()].emplace_back(
          i);
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
get_referenced_parameters(const Atom &atom, const Action &action) noexcept {
  std::vector<bool> parameter_matches(action.parameters.size(), false);
  for (size_t i = 0; i < atom.arguments.size(); ++i) {
    if (atom.arguments[i].is_parameter()) {
      parameter_matches[atom.arguments[i].get_parameter_index()] = true;
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
               const std::vector<Constant> &arguments) noexcept {
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
      [&problem](size_t product, Type type) {
        return product * problem.constants_of_type[type.id].size();
      });
}

inline size_t get_num_instantiated(const Action &action,
                                   const Problem &problem) noexcept {
  return std::accumulate(
      action.parameters.begin(), action.parameters.end(), 1ul,
      [&problem](size_t product, const Parameter &p) {
        if (!p.is_free()) {
          return product;
        }
        return product * problem.constants_of_type[p.get_type().id].size();
      });
}

inline size_t get_num_instantiated(const ParameterSelection &selection,
                                   const Action &action,
                                   const Problem &problem) noexcept {
  return std::accumulate(
      selection.begin(), selection.end(), 1ul,
      [&action, &problem](size_t product, ParameterIndex index) {
        auto type_index = action.parameters[index].get_type().id;
        return product * problem.constants_of_type[type_index].size();
      });
}

class AssignmentIterator {
  util::CombinationIterator combination_iterator_{};
  ParameterAssignment assignment_;
  const ParameterSelection *selection_ = nullptr;
  const Action *action_ = nullptr;
  const Problem *problem_ = nullptr;

  void set_assignment() {
    assert(combination_iterator_ != util::CombinationIterator{});
    const auto &combination = *combination_iterator_;
    for (size_t i = 0; i < combination.size(); ++i) {
      assert(action_->parameters[(*selection_)[i]].is_free());
      auto type_index = action_->parameters[(*selection_)[i]].get_type().id;
      assignment_[i] = std::make_pair(
          (*selection_)[i],
          problem_->constants_of_type[type_index][combination[i]]);
    }
  }

public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = ParameterAssignment;
  using difference_type = std::ptrdiff_t;
  using pointer = const value_type *;
  using reference = const value_type &;

  explicit AssignmentIterator() noexcept = default;

  explicit AssignmentIterator(const ParameterSelection &selection,
                              const Action &action,
                              const Problem &problem) noexcept
      : selection_{&selection}, action_{&action}, problem_{&problem} {
    std::vector<size_t> argument_size_list;
    argument_size_list.reserve(selection.size());
    for (auto p : selection) {
      assert(action.parameters[p].is_free());
      argument_size_list.push_back(
          problem.constants_of_type[action.parameters[p].get_type().id].size());
    }

    combination_iterator_ = util::CombinationIterator{argument_size_list};
    assignment_.resize(selection.size());

    if (combination_iterator_ != util::CombinationIterator{}) {
      set_assignment();
    }
  }

  AssignmentIterator &operator++() noexcept {
    assert(problem_);
    ++combination_iterator_;
    if (combination_iterator_ != util::CombinationIterator{}) {
      set_assignment();
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
};

class GroundAtomIterator {
  GroundAtom ground_atom_;
  ParameterMapping mapping_;
  AssignmentIterator assignment_iterator_;

  void set_ground_atom() {
    assert(assignment_iterator_ != AssignmentIterator{});
    const auto &assignment = *assignment_iterator_;
    for (size_t i = 0; i < mapping_.parameters.size(); ++i) {
      for (auto a : mapping_.arguments[i]) {
        ground_atom_.arguments[a] = assignment[i].second;
      }
    }
  }

public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = GroundAtom;
  using difference_type = std::ptrdiff_t;
  using pointer = const value_type *;
  using reference = const value_type &;

  explicit GroundAtomIterator() noexcept
      : ground_atom_{PredicateIndex{0}, {}} {}

  explicit GroundAtomIterator(const Atom &atom, const Action &action,
                              const Problem &problem) noexcept
      : ground_atom_{atom.predicate,
                     std::vector<Constant>(atom.arguments.size())},
        mapping_{get_mapping(atom, action)}, assignment_iterator_{
                                                 mapping_.parameters, action,
                                                 problem} {
    for (size_t i = 0; i < atom.arguments.size(); ++i) {
      if (!atom.arguments[i].is_parameter()) {
        ground_atom_.arguments[i] = atom.arguments[i].get_constant();
      }
    }

    if (assignment_iterator_ != AssignmentIterator{}) {
      set_ground_atom();
    }
  }

  GroundAtomIterator &operator++() noexcept {
    ++assignment_iterator_;
    if (assignment_iterator_ != AssignmentIterator{}) {
      set_ground_atom();
    }
    return *this;
  }

  GroundAtomIterator operator++(int) noexcept {
    auto old = *this;
    ++(*this);
    return old;
  }

  size_t get_num_instantiations() const noexcept {
    return assignment_iterator_.get_num_instantiations();
  }

  inline reference operator*() const noexcept { return ground_atom_; }

  const auto &get_assignment() const noexcept { return *assignment_iterator_; }

  bool operator!=(const GroundAtomIterator &) const noexcept {
    return assignment_iterator_ != AssignmentIterator{};
  }

  bool operator==(const GroundAtomIterator &) const noexcept {
    return assignment_iterator_ == AssignmentIterator{};
  }
};

inline bool is_instantiatable(const Atom &atom,
                              const std::vector<Constant> &arguments,
                              const Action &action,
                              const Problem &problem) noexcept {
  assert(atom.arguments.size() == arguments.size());
  if (is_ground(atom)) {
    for (size_t i = 0; i < atom.arguments.size(); ++i) {
      if (atom.arguments[i].get_constant().id != arguments[i].id) {
        return false;
      }
    }
    return true;
  }
  auto parameters = action.parameters;
  for (size_t i = 0; i < atom.arguments.size(); ++i) {
    if (!atom.arguments[i].is_parameter()) {
      if (atom.arguments[i].get_constant().id != arguments[i].id) {
        return false;
      }
    } else {
      auto &p = parameters[atom.arguments[i].get_parameter_index()];
      if (!p.is_free()) {
        if (p.get_constant().id != arguments[i].id) {
          return false;
        }
      } else {
        if (!is_subtype(arguments[i].type, p.get_type(), problem)) {
          return false;
        }
        p.set(arguments[i]);
      }
    }
  }
  return true;
}

inline bool is_unifiable(const Atom &first_atom, const Action &first_action,
                         const Atom &second_atom, const Action &second_action,
                         const Problem &problem) noexcept {
  assert(first_atom.predicate == second_atom.predicate);
  auto first_parameters = first_action.parameters;
  auto second_parameters = second_action.parameters;
  for (size_t i = 0; i < first_atom.arguments.size(); ++i) {
    const auto &first_p = first_atom.arguments[i];
    const auto &second_p = second_atom.arguments[i];
    if (!first_p.is_parameter() && !second_p.is_parameter()) {
      if (first_p.get_constant().id != second_p.get_constant().id) {
        return false;
      }
    } else if (!second_p.is_parameter()) {
      auto &action_p =
          first_parameters[first_atom.arguments[i].get_parameter_index()];
      if (!action_p.is_free()) {
        if (action_p.get_constant().id != second_p.get_constant().id) {
          return false;
        }
      } else {
        if (!is_subtype(second_p.get_constant().type, action_p.get_type(),
                        problem)) {
          return false;
        }
        action_p.set(second_p.get_constant());
      }
    } else if (!first_p.is_parameter()) {
      auto &action_p =
          second_parameters[second_atom.arguments[i].get_parameter_index()];
      if (!action_p.is_free()) {
        if (action_p.get_constant().id != first_p.get_constant().id) {
          return false;
        }
      } else {
        if (!is_subtype(first_p.get_constant().type, action_p.get_type(),
                        problem)) {
          return false;
        }
        action_p.set(first_p.get_constant());
      }
    }
  }
  return true;
}

} // namespace normalized

namespace std {

template <> struct hash<normalized::ParameterAssignment> {
  size_t operator()(const normalized::ParameterAssignment &assignment) const
      noexcept {
    size_t h = hash<size_t>{}(assignment.size());
    for (const auto &[parameter_index, constant] : assignment) {
      h ^= hash<normalized::ParameterIndex>{}(parameter_index);
      h ^= hash<normalized::ConstantIndex>{}(constant.id);
    }
    return h;
  }
};

} // namespace std

#endif /* end of include guard: NORMALIZED_UTILS_HPP */
