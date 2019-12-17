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

logging::Logger normalize_logger{"Normalize"};

normalized::Condition
normalize_atomic_condition(const parsed::BaseAtomicCondition &condition,
                           const parsed::Problem &problem) noexcept {
  normalized::Condition result;
  result.definition =
      normalized::PredicateIndex{problem.get_index(condition.get_predicate())};
  for (const auto &a : condition.get_arguments()) {
    if (auto c = std::get_if<const parsed::Constant *>(&a)) {
      result.arguments.emplace_back(
          normalized::ConstantIndex{problem.get_index(*c)});
    } else {
      assert(false);
    }
  }
  result.positive = condition.positive();

  return result;
}

normalized::Condition
normalize_atomic_condition(const parsed::BaseAtomicCondition &condition,
                           const parsed::Action &action,
                           const parsed::Problem &problem) noexcept {
  normalized::Condition result;
  result.definition =
      normalized::PredicateIndex{problem.get_index(condition.get_predicate())};
  for (const auto &a : condition.get_arguments()) {
    if (auto c = std::get_if<const parsed::Constant *>(&a)) {
      result.arguments.emplace_back(
          normalized::ConstantIndex{problem.get_index(*c)});
    } else if (auto p = std::get_if<const parsed::Parameter *>(&a)) {
      result.arguments.emplace_back(
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
normalize_action(const parsed::Action &action,
                 const parsed::Problem &problem) noexcept {
  std::vector<normalized::Action> new_actions;

  auto precondition =
      std::dynamic_pointer_cast<parsed::Condition>(action.precondition);
  assert(precondition);
  precondition->to_dnf();

  auto effect = std::dynamic_pointer_cast<parsed::Condition>(action.effect);
  assert(effect);
  auto effects = to_list(effect->to_dnf());

  if (effects.empty()) {
    return new_actions;
  }

  if (auto junction =
          std::dynamic_pointer_cast<parsed::BaseJunction>(precondition);
      junction && junction->get_operator() == parsed::JunctionOperator::Or) {
    if (junction->get_conditions().empty()) {
      return new_actions;
    }
    new_actions.reserve(junction->get_conditions().size());
    for (const auto &condition : junction->get_conditions()) {
      new_actions.push_back(normalized::Action{});
      auto &new_action = new_actions.back();
      for (const auto &p : action.parameters) {
        new_action.parameters.emplace_back(
            normalized::TypeIndex{problem.get_index(p->type)});
      }
      for (const auto &c : to_list(condition)) {
        auto condition = normalize_atomic_condition(*c, action, problem);
        if (is_grounded(condition)) {
          new_action.pre_instantiated.emplace_back(
              normalized::instantiate(condition), condition.positive);
        } else {
          new_action.preconditions.push_back(std::move(condition));
        }
      }
      for (const auto &c : effects) {
        auto condition = normalize_atomic_condition(*c, action, problem);
        if (is_grounded(condition)) {
          new_action.eff_instantiated.emplace_back(
              normalized::instantiate(condition), condition.positive);
        } else {
          new_action.effects.push_back(std::move(condition));
        }
      }
    }
  } else {
    new_actions.push_back(normalized::Action{});
    auto &new_action = new_actions.back();
    for (const auto &p : action.parameters) {
      new_action.parameters.emplace_back(
          normalized::TypeIndex{problem.get_index(p->type)});
    }
    for (const auto &c : to_list(precondition)) {
      auto condition = normalize_atomic_condition(*c, action, problem);
      if (is_grounded(condition)) {
        new_action.pre_instantiated.emplace_back(
            normalized::instantiate(condition), condition.positive);
      } else {
        new_action.preconditions.push_back(std::move(condition));
      }
    }
    for (const auto &c : effects) {
      auto condition = normalize_atomic_condition(*c, action, problem);
      if (is_grounded(condition)) {
        new_action.eff_instantiated.emplace_back(
            normalized::instantiate(condition), condition.positive);
      } else {
        new_action.effects.push_back(std::move(condition));
      }
    }
  }
  return new_actions;
}

normalized::Problem normalize(const parsed::Problem &problem) {
  normalized::Problem normalized_problem;
  normalized_problem.domain_name = problem.get_domain_name();
  normalized_problem.problem_name = problem.get_problem_name();
  normalized_problem.requirements = problem.get_requirements();

  normalized_problem.types.reserve(problem.get_types().size());
  normalized_problem.type_names.reserve(problem.get_types().size());
  for (size_t i = 0; i < problem.get_types().size(); ++i) {
    const auto &type = problem.get_types()[i];
    normalized_problem.types.push_back(normalized::Type{
        normalized::TypeIndex{problem.get_index(type->supertype)}});
    normalized_problem.type_names.push_back(type->name);
  }

  for (const auto &constant : problem.get_constants()) {
    normalized_problem.constants.push_back(normalized::Constant{
        normalized::TypeIndex{problem.get_index(constant->type)}});
    normalized_problem.constant_names.push_back(constant->name);
  }

  normalized_problem.constants_by_type.resize(normalized_problem.types.size());
  for (size_t i = 0; i < normalized_problem.constants.size(); ++i) {
    const auto &c = normalized_problem.constants[i];
    auto type = c.type;
    normalized_problem.constants_by_type[type].emplace_back(i);
    while (normalized_problem.get(type).supertype != type) {
      type = normalized_problem.get(type).supertype;
      normalized_problem.constants_by_type[type].emplace_back(i);
    }
  }

  for (const auto &predicate : problem.get_predicates()) {
    normalized_problem.predicates.emplace_back();
    for (const auto &type : predicate->parameter_types) {
      normalized_problem.predicates.back().parameter_types.emplace_back(
          problem.get_index(type));
    }
    normalized_problem.predicate_names.push_back(predicate->name);
  }

  std::vector<normalized::PredicateInstantiation> negative_init;
  for (const auto &predicate : problem.get_init()) {
    auto condition = normalize_atomic_condition(*predicate, problem);
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
      throw parsed::ModelException{"Found contradicting init predicate \'" +
                                   to_string(predicate, normalized_problem) +
                                   "\'"};
    }
  }

  // Reserve space for initial and equality predicates
  normalized_problem.init.reserve(normalized_problem.init.size() +
                                  normalized_problem.constants.size());
  for (size_t i = 0; i < normalized_problem.constants.size(); ++i) {
    normalized_problem.init.emplace_back(
        normalized::PredicateIndex{0},
        std::vector<normalized::ConstantIndex>{normalized::ConstantIndex{i},
                                               normalized::ConstantIndex{i}});
  }

  for (const auto &action : problem.get_actions()) {
    LOG_INFO(normalize_logger, "Normalizing action \'%s\'...",
             action->name.c_str());
    auto new_actions = normalize_action(*action, problem);
    LOG_INFO(normalize_logger, "resulted in %u STRIPS actions",
             new_actions.size());
    normalized_problem.actions.insert(normalized_problem.actions.cend(),
                                      new_actions.begin(), new_actions.end());
    normalized_problem.action_names.insert(
        normalized_problem.action_names.end(), action->name);
  }

  auto goal = std::dynamic_pointer_cast<parsed::Condition>(problem.get_goal())
                  ->to_dnf();
  for (const auto &predicate : to_list(goal)) {
    auto instantiation =
        instantiate(normalize_atomic_condition(*predicate, problem));
    if (auto it = std::find_if(normalized_problem.goal.begin(),
                               normalized_problem.goal.end(),
                               [&instantiation](const auto &g) {
                                 return instantiation == g.first;
                               });
        it != normalized_problem.goal.end()) {
      if (it->second == predicate->positive()) {
        LOG_WARN(normalize_logger, "Found duplicate goal predicate '%s'",
                 to_string(instantiation, normalized_problem).c_str());
      } else {
        throw parsed::ModelException{
            "Found conflicting goal predicates, problem unsolvable"};
      }
    }
    normalized_problem.goal.push_back(
        {std::move(instantiation), predicate->positive()});
  }
  LOG_DEBUG(normalize_logger, "Normalized problem:\n%s",
            to_string(normalized_problem).c_str());
  return normalized_problem;
}
