#include "model/problem.hpp"

#include <algorithm>
#include <list>
#include <string>
#include <variant>
#include <vector>

bool is_subtype(const Type *subtype, const Type *supertype) {
  if (subtype == supertype) {
    return true;
  }
  while (subtype->supertype != subtype) {
    subtype = subtype->supertype;
    if (subtype == supertype) {
      return true;
    }
  }
  return false;
}

void Predicate::add_parameter_type(const std::string &type) {
  parameter_types_.push_back(&problem_->get_type(type));
}

const std::vector<const Type *> &Predicate::get_parameter_types() const {
  return parameter_types_;
}

void AtomicCondition::add_bound_argument(const std::string &parameter) {
  if (finished_) {
    throw ModelException{"Internal model exception"};
  }
  if (!action_) {
    throw ModelException{"Variable arguments can only occur within actions"};
  }
  auto p = &action_->get_parameter(parameter);
  if (!is_subtype(p->type,
                  predicate_->get_parameter_types()[arguments_.size()])) {
    throw ModelException{
        "Type mismatch of bound argument \'" + parameter +
        "\': Expected a subtype of \'" +
        predicate_->get_parameter_types()[arguments_.size()]->name +
        "\' but got type \'" + p->type->name};
  }
  arguments_.push_back(p);
}

void AtomicCondition::add_constant_argument(const std::string &constant) {
  if (finished_) {
    throw ModelException{"Internal model exception"};
  }
  auto c = &problem_->get_constant(constant);
  if (!is_subtype(c->type,
                  predicate_->get_parameter_types()[arguments_.size()])) {
    throw ModelException{
        "Type mismatch of constant argument \'" + constant +
        "\': Expected a subtype of \'" +
        predicate_->get_parameter_types()[arguments_.size()]->name +
        "\' but got type \'" + c->type->name};
  }
  arguments_.push_back(c);
}

void AtomicCondition::finish() {
  if (finished_) {
    throw ModelException{"Internal model exception"};
  }
  if (arguments_.size() != predicate_->get_parameter_types().size()) {
    throw ModelException{
        "Too few arguments: Expected " +
        std::to_string(predicate_->get_parameter_types().size()) + " but got " +
        std::to_string(arguments_.size())};
  }
  finished_ = true;
}

AtomicCondition &
Conjunction::add_atomic_condition(bool positive, const std::string &predicate) {
  finish_prev();
  conditions_.emplace_back(
      AtomicCondition{positive, &problem_->get_predicate(predicate), problem_});
  return std::get<AtomicCondition>(conditions_.back());
}

Conjunction &Conjunction::add_conjuction(bool positive) {
  finish_prev();
  conditions_.emplace_back(Conjunction{positive, problem_});
  return std::get<Conjunction>(conditions_.back());
}

Disjunction &Conjunction::add_disjunction(bool positive) {
  finish_prev();
  conditions_.emplace_back(Disjunction{positive, problem_});
  return std::get<Disjunction>(conditions_.back());
}

void Conjunction::finish_prev() {
  if (!conditions_.empty()) {
    if (auto p = std::get_if<AtomicCondition>(&conditions_.back())) {
      p->finish();
    }
  }
}

AtomicCondition &
Disjunction::add_atomic_condition(bool positive, const std::string &predicate) {
  finish_prev();
  conditions_.emplace_back(
      AtomicCondition{positive, &problem_->get_predicate(predicate), problem_});
  return std::get<AtomicCondition>(conditions_.back());
}

Conjunction &Disjunction::add_conjuction(bool positive) {
  finish_prev();
  conditions_.emplace_back(Conjunction{positive, problem_});
  return std::get<Conjunction>(conditions_.back());
}

Disjunction &Disjunction::add_disjunction(bool positive) {
  finish_prev();
  conditions_.emplace_back(Disjunction{positive, problem_});
  return std::get<Disjunction>(conditions_.back());
}

void Disjunction::finish_prev() {
  if (!conditions_.empty()) {
    if (auto p = std::get_if<AtomicCondition>(&conditions_.back())) {
      p->finish();
    }
  }
}

const Parameter &Action::get_parameter(const std::string &name) const {
  auto it = std::find_if(parameters_.begin(), parameters_.end(),
                         [&name](const auto &p) { return p.name == name; });
  if (it == parameters_.end()) {
    throw ModelException{"Parameter \'" + name + "\' not found"};
  }
  return *it;
}

void Action::add_parameter(std::string name, const std::string &type) {
  if (std::find_if(parameters_.begin(), parameters_.end(),
                   [&name](const auto &t) { return t.name == name; }) !=
      parameters_.end()) {
    throw ModelException{"Parameter \'" + name + "\' already exists"};
  }
  parameters_.emplace_back(std::move(name), &problem_->get_type(type));
}

AtomicCondition &Action::set_atomic_precondition(bool negated,
                                                 const std::string &predicate) {
  precondition_ =
      AtomicCondition{negated, &problem_->get_predicate(predicate), problem_};
  return std::get<AtomicCondition>(precondition_);
}

Conjunction &Action::set_conjunctive_precondition(bool negated) {
  precondition_ = Conjunction{negated, problem_};
  return std::get<Conjunction>(precondition_);
}

Disjunction &Action::set_disjunctive_precondition(bool negated) {
  precondition_ = Disjunction{negated, problem_};
  return std::get<Disjunction>(precondition_);
}

AtomicCondition &Action::set_atomic_effect(bool negated,
                                           const std::string &predicate) {
  effect_ =
      AtomicCondition{negated, &problem_->get_predicate(predicate), problem_};
  return std::get<AtomicCondition>(effect_);
}

Conjunction &Action::set_conjunctive_effect(bool negated) {
  effect_ = Conjunction{negated, problem_};
  return std::get<Conjunction>(effect_);
}

Disjunction &Action::set_disjunctive_effect(bool negated) {
  effect_ = Disjunction{negated, problem_};
  return std::get<Disjunction>(effect_);
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
    throw ModelException{"Domain name must not be empty"};
  }
  if (domain_ref != domain_name_) {
    throw ModelException{"Domain reference does not match: Expected \'" +
                         domain_name_ + "\' but got \'" + domain_ref + "\'"};
  }
  problem_name_ = std::move(name);
}

void Problem::add_requirement(std::string name) {
  requirements_.push_back(std::move(name));
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

void Problem::add_type(std::string name, const std::string &supertype) {
  if (std::find_if(types_.begin(), types_.end(), [&name](const auto &t) {
        return t.name == name;
      }) != types_.end()) {
    throw ModelException{"Type \'" + name + "\' already exists"};
  }
  types_.emplace_back(std::move(name), &get_type(supertype));
}

const Constant &Problem::get_constant(const std::string &name) const {
  auto it = std::find(constants_.begin(), constants_.end(), name);
  if (it == constants_.end()) {
    throw ModelException{"Constant \'" + name + "\' not found"};
  }
  return *it;
}

void Problem::add_constant(std::string name, std::string type) {
  if (std::find_if(constants_.begin(), constants_.end(), [&name](const auto c) {
        return c.name == name;
      }) != constants_.end()) {
    throw ModelException{"Constant \'" + name + "\' already exists"};
  }
  constants_.emplace_back(std::move(name), &get_type(type));
}

const Predicate &Problem::get_predicate(const std::string &name) const {
  auto it = std::find_if(predicates_.begin(), predicates_.end(),
                         [&name](const auto &p) { return p.name == name; });
  if (it == predicates_.end()) {
    throw ModelException{"Predicate \'" + name + "\' not found"};
  }
  return *it;
}

Predicate &Problem::add_predicate(std::string name) {
  if (std::find_if(predicates_.begin(), predicates_.end(),
                   [&name](const auto &p) { return p.name == name; }) !=
      predicates_.end()) {
    throw ModelException{"Predicate \'" + name + "\' already exists"};
  }
  predicates_.emplace_back(std::move(name), this);
  return predicates_.back();
}

Action &Problem::add_action(std::string name) {
  if (std::find_if(actions_.begin(), actions_.end(), [&name](const auto &p) {
        return p.name == name;
      }) != actions_.end()) {
    throw ModelException{"Action \'" + name + "\' already exists"};
  }
  actions_.emplace_back(std::move(name), this);
  return actions_.back();
}
