#include "model/parsed/model.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace parsed {

void BaseAtomicCondition::check_complete() const {
  assert(arguments_.size() <= predicate_->parameter_types.size());
  if (arguments_.size() != predicate_->parameter_types.size()) {
    throw ModelException{"Too few arguments: Expected " +
                         std::to_string(predicate_->parameter_types.size()) +
                         " but got " + std::to_string(arguments_.size())};
  }
}

ParameterHandle Action::add_parameter(std::string name, TypeHandle type) {
  if (problem != type.get_base()) {
    throw ModelException{"Type is not from the same problem"};
  }
  if (std::find_if(parameters.begin(), parameters.end(),
                   [&name](const auto &p) { return p->name == name; }) !=
      parameters.end()) {
    throw ModelException{"Parameter \'" + name + "\' already exists"};
  }
  parameters.push_back(
      std::make_unique<Parameter>(Parameter{std::move(name), type.get()}));
  return {parameters.back().get(), this};
}

void Action::set_precondition(std::shared_ptr<Precondition> precondition) {
  if (problem != precondition->get_problem() ||
      this != precondition->get_action()) {
    throw ModelException{"Precondition not constructed for this action"};
  }
  if (auto p =
          dynamic_cast<AtomicCondition<ConditionContextType::Precondition> *>(
              precondition.get())) {
    p->check_complete();
  }
  this->precondition = std::move(precondition);
}

void Action::set_effect(std::shared_ptr<Effect> effect) {
  if (problem != effect->get_problem() || this != effect->get_action()) {
    throw ModelException{"Effect not constructed for this action"};
  }
  if (auto p = dynamic_cast<AtomicCondition<ConditionContextType::Effect> *>(
          effect.get())) {
    p->check_complete();
  }
  this->effect = std::move(effect);
}

ParameterHandle Action::get_parameter(const std::string &name) const {
  auto it = std::find_if(parameters.begin(), parameters.end(),
                         [&name](const auto &p) { return p->name == name; });
  if (it == parameters.end()) {
    throw ModelException{"Parameter \'" + name + "\' not found"};
  }
  return {it->get(), this};
}

void Problem::set_domain_name(std::string name) {
  if (name.empty()) {
    throw ModelException{"Domain name must not be empty"};
  }
  domain_name_ = std::move(name);
}

void Problem::set_problem_name(std::string name,
                               const std::string &domain_ref) {
  if (name.empty()) {
    throw ModelException{"Problem name must not be empty"};
  }
  if (domain_ref != domain_name_) {
    throw ModelException{"Domain reference does not match: Expected \'" +
                         domain_name_ + "\' but got \'" + domain_ref + "\'"};
  }
  problem_name_ = std::move(name);
}

TypeHandle Problem::add_type(std::string name) {
  if (std::find_if(types_.begin(), types_.end(), [&name](const auto &t) {
        return t->name == name;
      }) != types_.end()) {
    throw ModelException{"Type \'" + name + "\' already exists"};
  }
  types_.push_back(std::make_unique<Type>(Type{std::move(name), nullptr}));
  types_.back()->supertype = types_.back().get();
  return {types_.back().get(), this};
}

TypeHandle Problem::add_type(std::string name, TypeHandle supertype) {
  if (this != supertype.get_base()) {
    throw ModelException{"Supertype is not from this problem"};
  }
  if (std::find_if(types_.begin(), types_.end(), [&name](const auto &t) {
        return t->name == name;
      }) != types_.end()) {
    throw ModelException{"Type \'" + name + "\' already exists"};
  }
  types_.push_back(
      std::make_unique<Type>(Type{std::move(name), supertype.get()}));
  return {types_.back().get(), this};
}

ConstantHandle Problem::add_constant(std::string name, TypeHandle type) {
  if (this != type.get_base()) {
    throw ModelException{"Constant is not from this problem"};
  }
  if (std::find_if(constants_.begin(), constants_.end(),
                   [&name](const auto &c) { return c->name == name; }) !=
      constants_.end()) {
    throw ModelException{"Constant \'" + name + "\' already exists"};
  }
  constants_.push_back(
      std::make_unique<Constant>(Constant{std::move(name), type.get()}));
  return {constants_.back().get(), this};
}

PredicateHandle Problem::add_predicate(std::string name) {
  if (std::find_if(predicates_.begin(), predicates_.end(),
                   [&name](const auto &p) { return p->name == name; }) !=
      predicates_.end()) {
    throw ModelException{"Predicate \'" + name + "\' already exists"};
  }
  predicates_.push_back(std::make_unique<Predicate>(std::move(name)));
  return {predicates_.back().get(), this};
}

void Problem::add_parameter_type(PredicateHandle predicate, TypeHandle type) {
  if (this != predicate.get_base()) {
    throw ModelException{"Predicate is not from this problem"};
  }
  if (this != type.get_base()) {
    throw ModelException{"Type is not from this problem"};
  }
  predicate.p_->parameter_types.push_back(type.get());
}

ActionHandle Problem::add_action(std::string name) {
  if (std::find_if(actions_.begin(), actions_.end(), [&name](const auto &a) {
        return a->name == name;
      }) != actions_.end()) {
    throw ModelException{"Action \'" + name + "\' already exists"};
  }
  actions_.push_back(std::make_unique<Action>(Action{std::move(name), this}));
  return {actions_.back().get(), this};
}

ParameterHandle Problem::add_parameter(ActionHandle action, std::string name,
                                       TypeHandle type) {
  if (this != action.get_base()) {
    throw ModelException{"Action is not from this problem"};
  }
  return action.p_->add_parameter(std::move(name), type);
}

void Problem::set_precondition(ActionHandle action,
                               std::shared_ptr<Precondition> precondition) {
  if (this != action.get_base()) {
    throw ModelException{"Action is not from this problem"};
  }
  action.p_->set_precondition(std::move(precondition));
}

void Problem::set_effect(ActionHandle action, std::shared_ptr<Effect> effect) {
  if (this != action.get_base()) {
    throw ModelException{"Action is not from this problem"};
  }
  action.p_->set_effect(std::move(effect));
}

void Problem::add_init(std::shared_ptr<FreePredicate> init) {
  if (this != init->get_problem()) {
    throw ModelException{"Init predicate is not from this problem"};
  }
  init->check_complete();
  init_.push_back(std::move(init));
}

void Problem::set_goal(std::shared_ptr<GoalCondition> goal) {
  if (this != goal->get_problem()) {
    throw ModelException{"Goal is not from this problem"};
  }
  if (auto p = dynamic_cast<FreePredicate *>(goal.get())) {
    p->check_complete();
  }
  goal_ = std::move(goal);
}

TypeHandle Problem::get_type(const std::string &name) const {
  auto it = std::find_if(types_.begin(), types_.end(),
                         [&name](const auto &t) { return t->name == name; });
  if (it == types_.end()) {
    if (name != "object") {
      throw ModelException{"Type \'" + name + "\' not found"};
    } else {
      return {types_.front().get(), this};
    }
  }
  return {it->get(), this};
}

ConstantHandle Problem::get_constant(const std::string &name) const {
  auto it = std::find_if(constants_.begin(), constants_.end(),
                         [&name](const auto &c) { return c->name == name; });
  if (it == constants_.end()) {
    throw ModelException{"Constant \'" + name + "\' not found"};
  }
  return {it->get(), this};
}

PredicateHandle Problem::get_predicate(const std::string &name) const {
  auto it = std::find_if(predicates_.begin(), predicates_.end(),
                         [&name](const auto &p) { return p->name == name; });
  if (it == predicates_.end()) {
    throw ModelException{"Predicate \'" + name + "\' not found"};
  }
  return {it->get(), this};
}

ActionHandle Problem::get_action(const std::string &name) const {
  auto it = std::find_if(actions_.begin(), actions_.end(),
                         [&name](const auto &a) { return a->name == name; });
  if (it == actions_.end()) {
    throw ModelException{"Action \'" + name + "\' not found"};
  }
  return {it->get(), this};
}

} // namespace parsed
