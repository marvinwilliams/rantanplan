#include "model/normalize.hpp"
#include "logging/logging.hpp"
#include "model/normalized_problem.hpp"
#include "model/problem.hpp"
#include "model/to_string.hpp"
#include "model/utils.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <sstream>
#include <variant>
#include <vector>

logging::Logger normalize_logger{"Normalize"};

normalized::Condition
normalize_atomic_condition(const BaseAtomicCondition &condition) noexcept {
  normalized::Condition result;
  result.definition =
      normalized::PredicateHandle{condition.get_predicate()->id};
  for (const auto &a : condition.get_arguments()) {
    if (auto c = std::get_if<const Constant *>(&a)) {
      result.arguments.push_back({true, (*c)->id});
    } else if (auto p = std::get_if<const Parameter *>(&a)) {
      result.arguments.push_back({false, (*p)->id});
    } else {
      assert(false);
    }
  }
  result.positive = condition.positive();

  return result;
}

std::vector<std::shared_ptr<BaseAtomicCondition>>
to_list(std::shared_ptr<Condition> condition) noexcept {
  std::vector<std::shared_ptr<BaseAtomicCondition>> list;
  if (auto junction = std::dynamic_pointer_cast<BaseJunction>(condition)) {
    for (auto c : junction->get_conditions()) {
      list.push_back(std::static_pointer_cast<BaseAtomicCondition>(c));
    }
  } else {
    list.push_back(std::static_pointer_cast<BaseAtomicCondition>(condition));
  }
  return list;
}

std::vector<normalized::Action>
normalize_action(const Action &action) noexcept {
  std::vector<normalized::Action> new_actions;

  auto precondition =
      std::dynamic_pointer_cast<Condition>(action.precondition)->to_dnf();

  auto effects =
      to_list(std::dynamic_pointer_cast<Condition>(action.effect)->to_dnf());

  if (effects.empty()) {
    return new_actions;
  }

  std::vector<normalized::Condition> effect_list;

  for (const auto &e : effects) {
    effect_list.push_back(normalize_atomic_condition(*e));
  }

  if (auto junction = std::dynamic_pointer_cast<BaseJunction>(precondition);
      junction && junction->get_operator() == JunctionOperator::Or) {
    if (junction->get_conditions().empty()) {
      return new_actions;
    }
    new_actions.reserve(junction->get_conditions().size());
    for (const auto &condition : junction->get_conditions()) {
      normalized::Action new_action{};
      for (const auto &p : action.parameters) {
        new_action.parameters.push_back({false, p->type->id});
      }
      for (const auto &cond : to_list(condition)) {
        new_action.preconditions.push_back(normalize_atomic_condition(*cond));
      }
      new_action.effects = effect_list;
      new_actions.push_back(std::move(new_action));
    }
  } else {
    normalized::Action new_action{};
    for (const auto &p : action.parameters) {
      new_action.parameters.push_back({false, p->type->id});
    }
    for (const auto &cond : to_list(precondition)) {
      new_action.preconditions.push_back(normalize_atomic_condition(*cond));
    }
    new_action.effects = effect_list;
    new_actions.push_back(std::move(new_action));
  }
  return new_actions;
}

std::optional<normalized::Problem> normalize(const Problem &problem) noexcept {
  normalized::Problem normalized_problem{};
  normalized_problem.domain_name = problem.get_domain_name();
  normalized_problem.problem_name = problem.get_problem_name();
  normalized_problem.requirements = problem.get_requirements();

  for (const auto &type : problem.get_types()) {
    normalized_problem.types.push_back(
        {normalized::TypeHandle{type->supertype->id}});
    normalized_problem.type_names.push_back(type->name);
  }

  for (const auto &constant : problem.get_constants()) {
    normalized_problem.constants.push_back(
        {normalized::TypeHandle{constant->type->id}});
    normalized_problem.constant_names.push_back(constant->name);
  }

  for (const auto &predicate : problem.get_predicates()) {
    normalized_problem.predicates.emplace_back();
    for (const auto &type : predicate->parameter_types) {
      normalized_problem.predicates.back().parameter_types.push_back(
          {normalized::TypeHandle{type->id}});
    }
    normalized_problem.predicate_names.push_back(predicate->name);
  }

  std::vector<normalized::PredicateInstantiation> negative_init;
  for (const auto &predicate : problem.get_init()) {
    auto condition = normalize_atomic_condition(*predicate);
    auto instantiation = instantiate(condition);
    if (predicate->positive()) {
      if (std::find(normalized_problem.init.begin(),
                    normalized_problem.init.end(),
                    instantiation) == normalized_problem.init.end()) {
        normalized_problem.init.push_back(instantiation);
      } else {
        LOG_WARN(normalize_logger, "Found duplicate init predicate '%s'",
                 to_string(instantiation, normalized_problem).c_str());
      }
    } else {
      if (std::find(negative_init.begin(), negative_init.end(),
                    instantiation) == negative_init.end()) {
        negative_init.push_back(instantiation);
      } else {
        LOG_WARN(normalize_logger,
                 "Found duplicate negated init predicate '%s'",
                 to_string(instantiation, normalized_problem).c_str());
      }
    }
  }

  for (const auto &predicate : negative_init) {
    if (std::find(normalized_problem.init.begin(),
                  normalized_problem.init.end(),
                  predicate) != normalized_problem.init.end()) {
      LOG_ERROR(normalize_logger, "Found contradicting init predicate '%s'",
                to_string(predicate, normalized_problem).c_str());
      return std::nullopt;
    }
  }

  // Reserve space for initial and equality predicates
  normalized_problem.init.reserve(normalized_problem.init.size() +
                                  normalized_problem.constants.size());
  for (size_t i = 0; i < normalized_problem.constants.size(); ++i) {
    normalized_problem.init.push_back(normalized::PredicateInstantiation{
        normalized::PredicateHandle{0},
        {normalized::ConstantHandle{i}, normalized::ConstantHandle{i}}});
  }

  for (const auto &action : problem.get_actions()) {
    LOG_INFO(normalize_logger, "Normalizing action \'%s\'...",
             action->name.c_str());
    auto new_actions = normalize_action(*action);
    LOG_INFO(normalize_logger, "resulted in %u STRIPS actions",
             new_actions.size());
    normalized_problem.actions.insert(normalized_problem.actions.cend(),
                                      new_actions.begin(), new_actions.end());
    normalized_problem.action_names.insert(
        normalized_problem.action_names.end(), action->name);
  }

  auto goal =
      std::dynamic_pointer_cast<Condition>(problem.get_goal())->to_dnf();
  for (const auto &predicate : to_list(goal)) {
    normalized_problem.goal.push_back(
        {instantiate(normalize_atomic_condition(*predicate)),
         predicate->positive()});
  }
  return normalized_problem;
}
