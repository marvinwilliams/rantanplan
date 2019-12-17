#ifndef PARSED_MODEL_HPP
#define PARSED_MODEL_HPP

#include "model/parsed/model_exception.hpp"
#include "util/handle.hpp"

#include <algorithm>
#include <cassert>
#include <exception>
#include <iterator>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace parsed {

class Problem;

struct Type {
  std::string name;
  const Type *supertype;

  Type(std::string name, const Type *supertype)
      : name{std::move(name)}, supertype{supertype} {}
};

using TypeHandle = Handle<Type, Problem>;

inline bool is_subtype(const Type *subtype, const Type *supertype) {
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

struct Predicate {
  std::string name;
  std::vector<const Type *> parameter_types;

  Predicate(std::string name) : name{std::move(name)} {}
};

using PredicateHandle = Handle<Predicate, Problem>;

struct Constant {
  std::string name;
  const Type *type;

  Constant(std::string name, const Type *type)
      : name{std::move(name)}, type{type} {}
};

using ConstantHandle = Handle<Constant, Problem>;

struct Parameter {
  std::string name;
  const Type *type;

  Parameter(std::string name, const Type *type)
      : name{std::move(name)}, type{type} {}
};

struct Action;

using ActionHandle = Handle<Action, Problem>;
using ParameterHandle = Handle<Parameter, Action>;

/* template <> class ParameterHandle { */
/* public: */
/*   friend Action; */

/*   const Parameter *get() { return p_; } */
/*   const Action *get_action() { return action_; } */
/*   const Problem *get_problem() { return problem_; } */
/*   const Parameter &operator*() { return *p_; } */
/*   const Parameter *operator->() { return p_; } */
/*   bool operator==(const ParameterHandle &other) { */
/*     return p_ == other.p_ && action_ == other.action_ && */
/*            problem_ == other.problem_; */
/*   } */

/* private: */
/*   Handle(const Parameter *p, const Action *action, const Problem *problem) */
/*       : p_{p}, action_{action}, problem_{problem} {} */

/*   const Parameter *p_; */
/*   const Action *action_; */
/*   const Problem *problem_; */
/* }; */

class Condition : public std::enable_shared_from_this<Condition> {
public:
  friend class BaseJunction;
  friend class BaseAtomicCondition;

  bool positive() const { return positive_; }
  virtual std::shared_ptr<Condition> to_dnf() { return shared_from_this(); }
  virtual ~Condition() = default;

protected:
  explicit Condition(bool positive) : positive_{positive} {}

  bool positive_;
};

using Argument = std::variant<const Parameter *, const Constant *>;

class BaseAtomicCondition : public Condition {
public:
  explicit BaseAtomicCondition(bool positive, PredicateHandle predicate)
      : Condition{positive}, predicate_{predicate.get()} {}

  virtual void add_bound_argument(ParameterHandle parameter) {
    if (arguments_.size() == predicate_->parameter_types.size()) {
      throw parsed::ModelException{
          "Number of arguments exceeded: Predicate \'" + predicate_->name +
          "\' takes " + std::to_string(predicate_->parameter_types.size()) +
          " arguments"};
    }
    if (!is_subtype(parameter->type,
                    predicate_->parameter_types[arguments_.size()])) {
      throw ModelException{
          "Type mismatch of bound argument \'" + parameter->name +
          "\': Expected a subtype of \'" +
          predicate_->parameter_types[arguments_.size()]->name +
          "\' but got type \'" + parameter->type->name + "\'"};
    }
    arguments_.push_back(std::move(parameter.get()));
  }

  virtual void add_constant_argument(ConstantHandle constant) {
    if (arguments_.size() == predicate_->parameter_types.size()) {
      throw ModelException{"Number of arguments exceeded: Predicate \'" +
                           predicate_->name + "\' takes " +
                           std::to_string(predicate_->parameter_types.size()) +
                           " arguments"};
    }
    if (!is_subtype(constant->type,
                    predicate_->parameter_types[arguments_.size()])) {
      throw ModelException{
          "Type mismatch of constant argument \'" + constant->name +
          "\': Expected a subtype of \'" +
          predicate_->parameter_types[arguments_.size()]->name +
          "\' but got type \'" + constant->type->name + "\'"};
    }
    arguments_.push_back(std::move(constant.get()));
  }

  void check_complete() const;

  const Predicate *get_predicate() const { return predicate_; }
  const auto &get_arguments() const { return arguments_; }

  virtual ~BaseAtomicCondition() = default;

protected:
  const Predicate *predicate_;
  std::vector<Argument> arguments_;
};

enum class JunctionOperator { And, Or };

class BaseJunction : public Condition {
public:
  explicit BaseJunction(JunctionOperator op, bool positive)
      : Condition{positive}, op_{op} {}

  std::shared_ptr<Condition> to_dnf() override {
    if (!positive_) {
      for (auto &c : conditions_) {
        c->positive_ = !c->positive_;
      }
      op_ = (op_ == JunctionOperator::And ? JunctionOperator::Or
                                          : JunctionOperator::And);
      positive_ = true;
    }
    std::vector<std::shared_ptr<Condition>> new_conditions;
    for (auto &c : conditions_) {
      auto dnf = c->to_dnf();

      if (auto as_junction = std::dynamic_pointer_cast<BaseJunction>(dnf)) {
        assert(as_junction->positive_);
        if (as_junction->conditions_.empty()) {
          // Either skip or return at empty junctions
          if (op_ != as_junction->op_) {
            return std::make_shared<BaseJunction>(as_junction->op_, true);
          }
          continue;
        } else if (op_ == as_junction->op_) {
          // Flatten same child junctions
          new_conditions.insert(new_conditions.end(),
                                as_junction->conditions_.begin(),
                                as_junction->conditions_.end());
          continue;
        }
      }
      new_conditions.push_back(dnf);
    }
    conditions_ = new_conditions;
    if (conditions_.size() == 1) {
      return conditions_[0];
    }
    auto first_disjunction =
        std::find_if(conditions_.begin(), conditions_.end(), [](auto c) {
          if (auto as_junction = std::dynamic_pointer_cast<BaseJunction>(c)) {
            return as_junction->op_ == JunctionOperator::Or;
          }
          return false;
        });
    if (first_disjunction != conditions_.end()) {
      auto disjunction =
          std::dynamic_pointer_cast<BaseJunction>(*first_disjunction);
      conditions_.erase(first_disjunction);
      assert(disjunction->op_ == JunctionOperator::Or);
      auto new_disjunction =
          std::make_shared<BaseJunction>(JunctionOperator::Or, true);
      for (const auto &c : disjunction->conditions_) {
        auto sub_conjunction =
            std::make_shared<BaseJunction>(JunctionOperator::And, true);
        sub_conjunction->conditions_ = conditions_;
        sub_conjunction->conditions_.push_back(c);
        new_disjunction->conditions_.push_back(sub_conjunction);
      }
      return new_disjunction->to_dnf();
    }
    return shared_from_this();
  }

  virtual void add_condition(std::shared_ptr<Condition> condition) {
    if (auto p = dynamic_cast<BaseAtomicCondition *>(condition.get())) {
      p->check_complete();
    }
    conditions_.push_back(std::move(condition));
  }

  JunctionOperator get_operator() const { return op_; }
  const auto &get_conditions() const { return conditions_; }
  virtual ~BaseJunction() = default;

protected:
  JunctionOperator op_;
  std::vector<std::shared_ptr<Condition>> conditions_;
};

enum class ConditionContextType { Free, Precondition, Effect };

template <ConditionContextType C> class ConditionContext {
public:
  const Action *get_action() const { return action_; }
  const Problem *get_problem() const { return problem_; }

  virtual ~ConditionContext() = default;

protected:
  explicit ConditionContext(ActionHandle action)
      : action_{action.get()}, problem_{action.get_base()} {}

private:
  const Action *action_;
  const Problem *problem_;
};

template <> class ConditionContext<ConditionContextType::Free> {
public:
  explicit ConditionContext(const Problem *problem) : problem_{problem} {}

  const Problem *get_problem() const { return problem_; }

  virtual ~ConditionContext() = default;

private:
  const Problem *problem_;
};

template <ConditionContextType C>
class AtomicCondition final : public BaseAtomicCondition,
                              public ConditionContext<C> {
public:
  template <bool Enable = true, typename = std::enable_if_t<
                                    Enable && C == ConditionContextType::Free>>
  explicit AtomicCondition(bool positive, PredicateHandle predicate)
      : BaseAtomicCondition{positive, predicate}, ConditionContext<C>{
                                                      predicate.get_base()} {
    if (predicate->name == "=") {
      throw ModelException{"Predicate \'=\' can only be used in preconditions"};
    }
  }

  template <bool Enable = true, typename = std::enable_if_t<
                                    Enable && C != ConditionContextType::Free>>
  explicit AtomicCondition(bool positive, PredicateHandle predicate,
                           ActionHandle action)
      : BaseAtomicCondition{positive, predicate}, ConditionContext<C>{action} {
    if (predicate.get_base() != action.get_base()) {
      throw ModelException{
          "Predicate and action are not from the same problem"};
    }
    if (predicate->name == "=" && C != ConditionContextType::Precondition) {
      throw ModelException{"Predicate \'=\' can only be used in preconditions"};
    }
  }

  void add_bound_argument(ParameterHandle parameter) override {
    if constexpr (C == ConditionContextType::Free) {
      throw ModelException{"Bound arguments are only allowed within actions"};
    } else {
      if (ConditionContext<C>::get_action() != parameter.get_base()) {
        throw ModelException{"Parameter is not from the same action"};
      }
    }
    BaseAtomicCondition::add_bound_argument(parameter);
  }

  void add_constant_argument(ConstantHandle constant) override {
    if (ConditionContext<C>::get_problem() != constant.get_base()) {
      throw ModelException{"Constant is not from same problem"};
    }
    BaseAtomicCondition::add_constant_argument(constant);
  }
};

template <ConditionContextType C>
class Junction final : public BaseJunction, public ConditionContext<C> {
public:
  template <bool Enable = true, typename = std::enable_if_t<
                                    Enable && C == ConditionContextType::Free>>
  explicit Junction(JunctionOperator op, bool positive, const Problem *problem)
      : BaseJunction{op, positive}, ConditionContext<C>{problem} {}

  explicit Junction(JunctionOperator op, bool positive, ActionHandle action)
      : BaseJunction{op, positive}, ConditionContext<C>{action} {
    if (C == ConditionContextType::Effect) {
      if (op != JunctionOperator::And || !positive) {
        throw ModelException{"Only positive conjunctions allowed in effects"};
      }
    }
  }

  void add_condition(std::shared_ptr<Condition> condition) override {
    auto context = dynamic_cast<ConditionContext<C> *>(condition.get());
    if (!context) {
      throw ModelException{"Condition has not the same type"};
    }
    if (ConditionContext<C>::get_problem() != context->get_problem()) {
      throw ModelException{"Condition is not from the same problem"};
    }
    if constexpr (C != ConditionContextType::Free) {
      if (ConditionContext<C>::get_action() != context->get_action()) {
        throw ModelException{"Condition is not from the same action"};
      }
    }
    BaseJunction::add_condition(condition);
  }
};

using Precondition = ConditionContext<ConditionContextType::Precondition>;
using Effect = ConditionContext<ConditionContextType::Effect>;
using FreePredicate = AtomicCondition<ConditionContextType::Free>;
using GoalCondition = ConditionContext<ConditionContextType::Free>;

template <typename T>
size_t get_index(const T *elem,
                 const std::vector<std::unique_ptr<T>> &list) noexcept {
  return static_cast<size_t>(std::distance(
      list.begin(),
      std::find_if(list.begin(), list.end(),
                   [elem](const auto &e) { return e.get() == elem; })));
}

struct Action {
  friend Problem;

  std::string name;
  std::vector<std::unique_ptr<Parameter>> parameters;
  std::shared_ptr<Precondition> precondition;
  std::shared_ptr<Effect> effect;
  const Problem *problem;

  ParameterHandle add_parameter(std::string name, TypeHandle type);
  void set_precondition(std::shared_ptr<Precondition> precondition);
  void set_effect(std::shared_ptr<Effect> effect);
  ParameterHandle get_parameter(const std::string &name) const;

  size_t get_index(const Parameter *parameter) const noexcept {
    return parsed::get_index(parameter, parameters);
  }

private:
  Action(std::string name, const Problem *problem)
      : name{std::move(name)}, problem{problem} {}
};

class Problem {
public:
  void set_domain_name(std::string name);
  void set_problem_name(std::string name, const std::string &domain_ref);
  void add_requirement(std::string name) {
    requirements_.push_back(std::move(name));
  }
  TypeHandle add_type(std::string name);
  TypeHandle add_type(std::string name, TypeHandle supertype);
  ConstantHandle add_constant(std::string name, TypeHandle type);
  PredicateHandle add_predicate(std::string name);
  void add_parameter_type(PredicateHandle predicate, TypeHandle type);
  ActionHandle add_action(std::string name);
  ParameterHandle add_parameter(ActionHandle action, std::string name,
                                TypeHandle type);
  void set_precondition(ActionHandle action,
                        std::shared_ptr<Precondition> precondition);
  void set_effect(ActionHandle action, std::shared_ptr<Effect> effect);
  void add_init(std::shared_ptr<FreePredicate> init);
  void set_goal(std::shared_ptr<GoalCondition> goal);

  const std::string &get_domain_name() const { return domain_name_; }
  const std::string &get_problem_name() const { return problem_name_; }
  const std::vector<std::string> &get_requirements() const {
    return requirements_;
  }
  const auto &get_types() const { return types_; }
  const auto &get_constants() const { return constants_; }
  const auto &get_predicates() const { return predicates_; }
  const auto &get_actions() const { return actions_; }
  TypeHandle get_type(const std::string &name) const;
  ConstantHandle get_constant(const std::string &name) const;
  PredicateHandle get_predicate(const std::string &name) const;
  ActionHandle get_action(const std::string &name) const;
  const auto &get_init() const { return init_; }
  const auto &get_goal() const { return goal_; }

  size_t get_index(const Type *type) const noexcept {
    return parsed::get_index(type, types_);
  }

  size_t get_index(const Constant *constant) const noexcept {
    return parsed::get_index(constant, constants_);
  }

  size_t get_index(const Predicate *predicate) const noexcept {
    return parsed::get_index(predicate, predicates_);
  }

  size_t get_index(const Action *action) const noexcept {
    return parsed::get_index(action, actions_);
  }

private:
  std::string domain_name_ = "";
  std::string problem_name_ = "";
  std::vector<std::string> requirements_;
  std::vector<std::unique_ptr<Type>> types_;
  std::vector<std::unique_ptr<Constant>> constants_;
  std::vector<std::unique_ptr<Predicate>> predicates_;
  std::vector<std::unique_ptr<Action>> actions_;
  std::vector<std::shared_ptr<FreePredicate>> init_;
  std::shared_ptr<GoalCondition> goal_;
};

} // namespace parsed

#endif /* end of include guard: PARSED_MODEL_HPP */
