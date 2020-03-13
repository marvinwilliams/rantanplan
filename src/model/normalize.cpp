#include "model/normalize.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "model/parsed/model.hpp"
#include "model/to_string.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <sstream>
#include <variant>
#include <vector>

normalized::Condition normalize_atomic_condition(
    const parsed::BaseAtomicCondition &condition,
    const parsed::Problem &problem,
    const normalized::Problem &normalized_problem) noexcept {
  normalized::Condition result;
  result.atom.predicate =
      normalized::PredicateIndex{problem.get_index(condition.get_predicate())};
  for (const auto &a : condition.get_arguments()) {
    assert(std::holds_alternative<const parsed::Constant *>(a));
    auto c = std::get<const parsed::Constant *>(a);
    result.atom.arguments.emplace_back(
        normalized_problem.constants[problem.get_index(c)]);
  }
  result.positive = condition.positive();

  return result;
}

normalized::Condition normalize_atomic_condition(
    const parsed::BaseAtomicCondition &condition, const parsed::Action &action,
    const parsed::Problem &problem,
    const normalized::Problem &normalized_problem) noexcept {
  normalized::Condition result;
  result.atom.predicate =
      normalized::PredicateIndex{problem.get_index(condition.get_predicate())};
  for (const auto &a : condition.get_arguments()) {
    if (auto c = std::get_if<const parsed::Constant *>(&a)) {
      result.atom.arguments.emplace_back(
          normalized_problem.constants[problem.get_index(*c)]);
    } else if (auto p = std::get_if<const parsed::Parameter *>(&a)) {
      result.atom.arguments.emplace_back(
          normalized::ParameterIndex{action.get_index(*p)});
    } else {
      assert(false);
    }
  }
  result.positive = condition.positive();

  return result;
}

std::vector<std::shared_ptr<parsed::BaseAtomicCondition>>
to_list(std::shared_ptr<parsed::Condition> condition) noexcept {
  std::vector<std::shared_ptr<parsed::BaseAtomicCondition>> list;
  if (auto junction =
          std::dynamic_pointer_cast<parsed::BaseJunction>(condition)) {
    for (auto c : junction->get_conditions()) {
      list.push_back(std::static_pointer_cast<parsed::BaseAtomicCondition>(c));
    }
  } else {
    list.push_back(
        std::static_pointer_cast<parsed::BaseAtomicCondition>(condition));
  }
  return list;
}

std::vector<normalized::Action>
normalize_action(const parsed::Action &action, const parsed::Problem &problem,
                 const normalized::Problem &normalized_problem) noexcept {
  auto precondition =
      std::dynamic_pointer_cast<parsed::Condition>(action.precondition);

  auto effect = std::dynamic_pointer_cast<parsed::Condition>(action.effect);

  if (!precondition || !effect) {
    return {};
  }

  precondition->to_dnf();

  auto effects = to_list(effect->to_dnf());

  if (effects.empty()) {
    return {};
  }

  std::vector<normalized::Action> new_actions;

  if (auto junction =
          std::dynamic_pointer_cast<parsed::BaseJunction>(precondition);
      junction && junction->get_operator() == parsed::JunctionOperator::Or) {
    if (junction->get_conditions().empty()) {
      return {};
    }
    new_actions.reserve(junction->get_conditions().size());
    for (const auto &condition : junction->get_conditions()) {
      new_actions.push_back(normalized::Action{});
      auto &new_action = new_actions.back();
      for (const auto &p : action.parameters) {
        new_action.parameters.emplace_back(
            normalized_problem.types[problem.get_index(p->type)]);
      }
      for (const auto &c : to_list(condition)) {
        new_action.preconditions.push_back(normalize_atomic_condition(
            *c, action, problem, normalized_problem));
      }
      for (const auto &c : effects) {
        new_action.effects.push_back(normalize_atomic_condition(
            *c, action, problem, normalized_problem));
      }
    }
  } else {
    new_actions.push_back(normalized::Action{});
    auto &new_action = new_actions.back();
    for (const auto &p : action.parameters) {
      new_action.parameters.emplace_back(
          normalized_problem.types[problem.get_index(p->type)]);
    }
    for (const auto &c : to_list(precondition)) {
      new_action.preconditions.push_back(
          normalize_atomic_condition(*c, action, problem, normalized_problem));
    }
    for (const auto &c : effects) {
      new_action.effects.push_back(
          normalize_atomic_condition(*c, action, problem, normalized_problem));
    }
  }

  return new_actions;
}

std::shared_ptr<normalized::Problem> normalize(const parsed::Problem &problem) {
  auto normalized_problem = std::make_shared<normalized::Problem>();

  normalized_problem->domain_name = problem.get_domain_name();
  normalized_problem->problem_name = problem.get_problem_name();
  normalized_problem->requirements = problem.get_requirements();
  normalized_problem->types.reserve(problem.get_types().size());
  normalized_problem->type_names.reserve(problem.get_types().size());

  for (const auto &t : problem.get_types()) {
    normalized_problem->types.push_back(normalized::Type{
        normalized::TypeIndex{problem.get_index(t.get())},
        normalized::TypeIndex{problem.get_index(t->supertype)}});
    normalized_problem->type_names.push_back(t->name);
  }

  for (const auto &c : problem.get_constants()) {
    normalized_problem->constants.push_back(normalized::Constant{
        normalized::ConstantIndex{problem.get_index(c.get())},
        normalized_problem->types[problem.get_index(c->type)]});
    normalized_problem->constant_names.push_back(c->name);
  }

  normalized_problem->constants_of_type.resize(
      normalized_problem->types.size());
  normalized_problem->constant_type_map.resize(
      normalized_problem->types.size());
  for (const auto &c : normalized_problem->constants) {
    auto type = c.type;
    normalized_problem->constants_of_type[type.id].emplace_back(c);
    normalized_problem->constant_type_map[type.id][c.id] =
        normalized_problem->constants_of_type[type.id].size();
    while (type.supertype != type.id) {
      type = normalized_problem->types[type.supertype];
      normalized_problem->constants_of_type[type.id].emplace_back(c);
      normalized_problem->constant_type_map[type.id][c.id] =
          normalized_problem->constants_of_type[type.id].size();
    }
  }

  for (const auto &predicate : problem.get_predicates()) {
    normalized_problem->predicates.emplace_back();
    normalized_problem->predicates.back().id =
        normalized::PredicateIndex{problem.get_index(predicate.get())};
    for (const auto &t : predicate->parameter_types) {
      normalized_problem->predicates.back().parameter_types.push_back(
          normalized_problem->types[problem.get_index(t)]);
    }
    normalized_problem->predicate_names.push_back(predicate->name);
  }

  LOG_INFO(normalize_logger, "Normalizing init...");

  std::vector<normalized::GroundAtom> negative_init;
  for (const auto &init : problem.get_init()) {
    auto ground_atom = as_ground_atom(
        normalize_atomic_condition(*init, problem, *normalized_problem).atom);
    if (init->positive()) {
      if (std::find(normalized_problem->init.begin(),
                    normalized_problem->init.end(),
                    ground_atom) == normalized_problem->init.end()) {
        normalized_problem->init.push_back(ground_atom);
      } else {
        LOG_WARN(normalize_logger, "Found duplicate init atom '%s'",
                 to_string(ground_atom, *normalized_problem).c_str());
      }
    } else {
      if (std::find(negative_init.begin(), negative_init.end(), ground_atom) ==
          negative_init.end()) {
        negative_init.push_back(ground_atom);
      } else {
        LOG_WARN(normalize_logger, "Found duplicate negated init atom '%s'",
                 to_string(ground_atom, *normalized_problem).c_str());
      }
    }
  }

  for (const auto &atom : negative_init) {
    if (std::find(normalized_problem->init.begin(),
                  normalized_problem->init.end(),
                  atom) != normalized_problem->init.end()) {
      LOG_ERROR(normalize_logger, "Found conflicting init atom '%s'",
                to_string(atom, *normalized_problem).c_str());
      return std::shared_ptr<normalized::Problem>();
    }
  }

  // Reserve space for initial and equality predicates
  normalized_problem->init.reserve(normalized_problem->init.size() +
                                   normalized_problem->constants.size());
  for (const auto &c : normalized_problem->constants) {
    normalized_problem->init.push_back(
        normalized::GroundAtom{normalized::PredicateIndex{0},
                               std::vector<normalized::Constant>{c, c}});
  }

  LOG_INFO(normalize_logger, "Normalizing goal...");

  auto goal_condition =
      std::dynamic_pointer_cast<parsed::Condition>(problem.get_goal())
          ->to_dnf();
  for (const auto &goal : to_list(goal_condition)) {
    auto ground_atom = as_ground_atom(
        normalize_atomic_condition(*goal, problem, *normalized_problem).atom);
    if (auto it = std::find_if(
            normalized_problem->goal.begin(), normalized_problem->goal.end(),
            [&ground_atom](const auto &g) { return ground_atom == g.first; });
        it != normalized_problem->goal.end()) {
      if (it->second == goal->positive()) {
        LOG_WARN(normalize_logger, "Found duplicate goal predicate '%s'",
                 to_string(ground_atom, *normalized_problem).c_str());
      } else {
        LOG_ERROR(normalize_logger, "Found conflicting goal predicates '%s'",
                  to_string(ground_atom, *normalized_problem).c_str());
        return std::shared_ptr<normalized::Problem>();
      }
    }

    normalized_problem->goal.push_back(
        {std::move(ground_atom), goal->positive()});
  }

  LOG_INFO(normalize_logger, "Normalizing actions...");

  for (const auto &action : problem.get_actions()) {
    auto new_actions = normalize_action(*action, problem, *normalized_problem);
    for (const auto &a : new_actions) {
      if (get_num_instantiated(a, *normalized_problem) > 0) {
        normalized_problem->actions.push_back(a);
        normalized_problem->actions.back().id =
            normalized_problem->actions.size() - 1;
        normalized_problem->action_names.push_back(action->name);
      }
    }
  }

  return normalized_problem;
}
