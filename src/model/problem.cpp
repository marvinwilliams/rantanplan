#include "model/problem.hpp"

#include <algorithm>
#include <cassert>
#include <list>
#include <string>
#include <variant>
#include <vector>

bool is_subtype(const Type *subtype, const Type *supertype) {
  if (subtype == supertype) {
    return true;
  }
  while (subtype->supertype != nullptr) {
    subtype = subtype->supertype;
    if (subtype == supertype) {
      return true;
    }
  }
  return false;
}

Predicate &Predicate::add_parameter_type(const std::string &type) {
  parameter_types_.push_back(&problem->get_type(type));
  return *this;
}

const auto &Predicate::get_parameter_types() const { return parameter_types_; }

AtomicCondition::AtomicCondition(bool positive, ConditionContext context,
                                 const Predicate *predicate,
                                 const Action *action, const Problem *problem)
    : positive{positive}, context{context}, predicate{predicate},
      action{action}, problem{problem} {
  if (action && context != ConditionContext::Precondition &&
      context != ConditionContext::Effect) {
    throw ModelException{"Invalid condition context"};
  } else if (!action && context != ConditionContext::Init &&
             context != ConditionContext::Goal) {
    throw ModelException{"Invalid condition context"};
  }
  if (predicate->name == "=" && context != ConditionContext::Precondition) {
    throw ModelException{"Predicate \"=\" can only be used in preconditions"};
  }
}

AtomicCondition &
AtomicCondition::add_bound_argument(const std::string &parameter) {
  if (context != ConditionContext::Precondition &&
      context != ConditionContext::Effect) {
    throw ModelException{"Variable arguments can only occur within actions"};
  }
  assert(action);
  if (arguments_.size() == predicate->get_parameter_types().size()) {
    throw ModelException{
        "Number of arguments exceeded: Predicate takes " +
        std::to_string(predicate->get_parameter_types().size()) + " arguments"};
  }
  auto p = &action->get_parameter(parameter);
  if (!is_subtype(p->type,
                  predicate->get_parameter_types()[arguments_.size()])) {
    throw ModelException{
        "Type mismatch of bound argument \'" + parameter +
        "\': Expected a subtype of \'" +
        predicate->get_parameter_types()[arguments_.size()]->name +
        "\' but got type \'" + p->type->name};
  }
  arguments_.push_back(p);
  return *this;
}

AtomicCondition &
AtomicCondition::add_constant_argument(const std::string &constant) {
  if (arguments_.size() == predicate->get_parameter_types().size()) {
    throw ModelException{
        "Number of arguments exceeded: Predicate takes " +
        std::to_string(predicate->get_parameter_types().size()) + " arguments"};
  }
  auto c = &problem->get_constant(constant);
  if (!is_subtype(c->type,
                  predicate->get_parameter_types()[arguments_.size()])) {
    throw ModelException{
        "Type mismatch of constant argument \'" + constant +
        "\': Expected a subtype of \'" +
        predicate->get_parameter_types()[arguments_.size()]->name +
        "\' but got type \'" + c->type->name};
  }
  arguments_.push_back(c);
  return *this;
}

const auto &AtomicCondition::get_arguments() const { return arguments_; }

void AtomicCondition::check_complete() const {
  assert(arguments_.size() <= predicate->get_parameter_types().size());
  if (arguments_.size() != predicate->get_parameter_types().size()) {
    throw ModelException{
        "Too few arguments: Expected " +
        std::to_string(predicate->get_parameter_types().size()) + " but got " +
        std::to_string(arguments_.size())};
  }
}

Junction::Junction(Operator op, bool positive, ConditionContext context,
                   const Action *action, const Problem *problem)
    : op{op}, positive{positive}, context{context}, action{action},
      problem{problem} {
  if (context == ConditionContext::Init) {
    throw ModelException{"Invalid condition context"};
  }

  if (action && context != ConditionContext::Precondition &&
      context != ConditionContext::Effect) {
    throw ModelException{"Invalid condition context"};
  } else if (!action && context != ConditionContext::Init &&
             context != ConditionContext::Goal) {
    throw ModelException{"Invalid condition context"};
  }

  if ((!positive || op == Operator::Or) &&
      context == ConditionContext::Effect) {
    throw ModelException{
        "Only positive conjunctions are not allowed in effects"};
  }
}

Junction &Junction::add_condition(Condition condition) {
  if (!std::visit(
          [this](const auto &c) {
            return c.context == context && c.action == action &&
                   c.problem == problem;
          },
          condition)) {
    throw ModelException{"Condition must be from the same context"};
  }
  conditions_.push_back(std::move(condition));
  return *this;
}

const auto &Junction::get_conditions() const { return conditions_; }

const Parameter &Action::get_parameter(const std::string &name) const {
  auto it = std::find_if(parameters_.begin(), parameters_.end(),
                         [&name](const auto &p) { return p.name == name; });
  if (it == parameters_.end()) {
    throw ModelException{"Parameter \'" + name + "\' not found"};
  }
  return *it;
}

Action &Action::add_parameter(std::string name, const std::string &type) {
  if (std::find_if(parameters_.begin(), parameters_.end(),
                   [&name](const auto &t) { return t.name == name; }) !=
      parameters_.end()) {
    throw ModelException{"Parameter \'" + name + "\' already exists"};
  }
  parameters_.emplace_back(std::move(name), &problem->get_type(type));
  return *this;
}

void Action::set_precondition(Condition condition) {
  if (!std::visit(
          [this](const auto &c) {
            return c.context == ConditionContext::Precondition &&
                   c.action == this && c.problem == problem;
          },
          condition)) {
    throw ModelException{"Condition must be from the same context"};
  }
}

void Action::set_effect(Condition condition) {
  if (!std::visit(
          [this](const auto &c) {
            return c.context == ConditionContext::Effect && c.action == this &&
                   c.problem == problem;
          },
          condition)) {
    throw ModelException{"Condition must be from the same context"};
  }
}

const auto &Action::get_parameters() const { return parameters_; }
const Condition &Action::get_precondition() const { return precondition_; }
const Condition &Action::get_effect() const { return effect_; }

void Problem::set_domain_name(std::string name) {
  if (name.empty()) {
    throw ModelException{"Domain name must not be empty"};
  }
  domain_name_ = std::move(name);
}

void Problem::set_problem_name(std::string name,
                               const std::string &domain_ref) {
  if (name.empty()) {
    throw ModelException{"Domain name must not be empty"};
  }
  if (domain_ref != domain_name_) {
    throw ModelException{"Domain reference does not match: Expected \'" +
                         domain_name_ + "\' but got \'" + domain_ref + "\'"};
  }
  problem_name_ = std::move(name);
}

Problem &Problem::add_requirement(std::string name) {
  requirements_.push_back(std::move(name));
  return *this;
}

const Type &Problem::get_type(const std::string &name) const {
  auto it = std::find(types_.begin(), types_.end(), name);
  if (it == types_.end()) {
    if (name != "object") {
      throw ModelException{"Type \'" + name + "\' not found"};
    } else {
      return types_.front();
    }
  }
  return *it;
}

const Type &Problem::get_root_type() const { return *types_.begin(); }

Problem &Problem::add_type(std::string name, const std::string &supertype) {
  if (std::find_if(types_.begin(), types_.end(), [&name](const auto &t) {
        return t.name == name;
      }) != types_.end()) {
    throw ModelException{"Type \'" + name + "\' already exists"};
  }
  types_.emplace_back(std::move(name), &get_type(supertype));
  return *this;
}

const Constant &Problem::get_constant(const std::string &name) const {
  auto it = std::find(constants_.begin(), constants_.end(), name);
  if (it == constants_.end()) {
    throw ModelException{"Constant \'" + name + "\' not found"};
  }
  return *it;
}

Problem &Problem::add_constant(std::string name, std::string type) {
  if (std::find_if(constants_.begin(), constants_.end(), [&name](const auto c) {
        return c.name == name;
      }) != constants_.end()) {
    throw ModelException{"Constant \'" + name + "\' already exists"};
  }
  constants_.emplace_back(std::move(name), &get_type(type));
  return *this;
}

const Predicate &Problem::get_predicate(const std::string &name) const {
  auto it = std::find_if(predicates_.begin(), predicates_.end(),
                         [&name](const auto &p) { return p.name == name; });
  if (it == predicates_.end()) {
    throw ModelException{"Predicate \'" + name + "\' not found"};
  }
  return *it;
}

Problem &Problem::add_predicate(Predicate predicate) {
  if (predicate.problem != this) {
    throw ModelException{"Predicate must be from the same context"};
  }
  if (std::find_if(predicates_.begin(), predicates_.end(),
                   [&name = predicate.name](const auto &p) {
                     return p.name == name;
                   }) != predicates_.end()) {
    throw ModelException{"Predicate \'" + predicate.name + "\' already exists"};
  }
  predicates_.push_back(std::move(predicate));
  return *this;
}

Problem &Problem::add_action(Action action) {
  if (action.problem != this) {
    throw ModelException{"Action must be from the same context"};
  }
  if (std::find_if(actions_.begin(), actions_.end(),
                   [&name = action.name](const auto &p) {
                     return p.name == name;
                   }) != actions_.end()) {
    throw ModelException{"Action \'" + action.name + "\' already exists"};
  }
  actions_.emplace_back(std::move(action));
  return *this;
}
