#ifndef PROBLEM_HPP
#define PROBLEM_HPP

#include <algorithm>
#include <cassert>
#include <exception>
#include <memory>
#include <string>
#include <variant>
#include <vector>

class ModelException : public std::exception {
public:
  explicit ModelException(std::string message) : message_{std::move(message)} {}

  [[nodiscard]] inline const char *what() const noexcept override {
    return message_.c_str();
  }

private:
  std::string message_;
};

class Problem;

template <typename T> class Handle {
public:
  friend Problem;

  explicit Handle() : p_{nullptr}, problem_{nullptr} {}

  const T *get() { return p_; }
  const Problem *get_problem() { return problem_; }
  const T &operator*() { return *p_; }
  const T *operator->() { return p_; }
  bool operator==(const Handle<T> &other) const {
    return p_ == other.p_ && problem_ == other.problem_;
  }

private:
  Handle(T *p, const Problem *problem) : p_{p}, problem_{problem} {}

  T *p_;
  const Problem *problem_;
};

struct Type {
  friend Problem;

  std::string name;
  const Type *supertype;
  size_t id;

private:
  Type(std::string name, const Type *supertype, size_t id)
      : name{std::move(name)}, supertype{supertype}, id{id} {}
};

bool is_subtype(const Type *first, const Type *second);

struct Predicate {
  friend Problem;

  std::string name;
  std::vector<const Type *> parameter_types;
  size_t id;

private:
  Predicate(std::string name, size_t id) : name{std::move(name)}, id{id} {}
};

struct Constant {
  friend Problem;

  std::string name;
  const Type *type;
  size_t id;

private:
  Constant(std::string name, const Type *type, size_t id)
      : name{std::move(name)}, type{type}, id{id} {}
};

struct Action;

struct Parameter {
  friend Action;

  std::string name;
  const Type *type;
  size_t id;

private:
  Parameter(std::string name, const Type *type, size_t id)
      : name{std::move(name)}, type{type}, id{id} {}
};

template <> class Handle<Parameter> {
public:
  friend Action;

  const Parameter *get() { return p_; }
  const Action *get_action() { return action_; }
  const Problem *get_problem() { return problem_; }
  const Parameter &operator*() { return *p_; }
  const Parameter *operator->() { return p_; }
  bool operator==(const Handle<Parameter> &other) {
    return p_ == other.p_ && action_ == other.action_ &&
           problem_ == other.problem_;
  }

private:
  Handle(const Parameter *p, const Action *action, const Problem *problem)
      : p_{p}, action_{action}, problem_{problem} {}

  const Parameter *p_;
  const Action *action_;
  const Problem *problem_;
};

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
  explicit BaseAtomicCondition(bool positive, Handle<Predicate> predicate)
      : Condition{positive}, predicate_{predicate.get()} {}

  virtual void add_bound_argument(Handle<Parameter> parameter) {
    if (arguments_.size() == predicate_->parameter_types.size()) {
      throw ModelException{"Number of arguments exceeded: Predicate \'" +
                           predicate_->name + "\' takes " +
                           std::to_string(predicate_->parameter_types.size()) +
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

  virtual void add_constant_argument(Handle<Constant> constant) {
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
  explicit ConditionContext(Handle<Action> action)
      : action_{action.get()}, problem_{action.get_problem()} {}

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
class AtomicCondition : public BaseAtomicCondition, public ConditionContext<C> {
public:
  template <bool Enable = true, typename = std::enable_if_t<
                                    Enable && C == ConditionContextType::Free>>
  explicit AtomicCondition(bool positive, Handle<Predicate> predicate)
      : BaseAtomicCondition{positive, predicate}, ConditionContext<C>{
                                                      predicate.get_problem()} {
    if (predicate->name == "=") {
      throw ModelException{"Predicate \'=\' can only be used in preconditions"};
    }
  }

  template <bool Enable = true, typename = std::enable_if_t<
                                    Enable && C != ConditionContextType::Free>>
  explicit AtomicCondition(bool positive, Handle<Predicate> predicate,
                           Handle<Action> action)
      : BaseAtomicCondition{positive, predicate}, ConditionContext<C>{action} {
    if (predicate.get_problem() != action.get_problem()) {
      throw ModelException{
          "Predicate and action are not from the same problem"};
    }
    if (predicate->name == "=" && C != ConditionContextType::Precondition) {
      throw ModelException{"Predicate \'=\' can only be used in preconditions"};
    }
  }

  void add_bound_argument(Handle<Parameter> parameter) override {
    if constexpr (C == ConditionContextType::Free) {
      throw ModelException{"Bound arguments are only allowed within actions"};
    } else {
      if (ConditionContext<C>::get_action() != parameter.get_action()) {
        throw ModelException{"Parameter is not from the same action"};
      }
    }
    BaseAtomicCondition::add_bound_argument(parameter);
  }

  void add_constant_argument(Handle<Constant> constant) override {
    if (ConditionContext<C>::get_problem() != constant.get_problem()) {
      throw ModelException{"Constant is not from same problem"};
    }
    BaseAtomicCondition::add_constant_argument(constant);
  }
};

template <ConditionContextType C>
class Junction : public BaseJunction, public ConditionContext<C> {
public:
  template <bool Enable = true, typename = std::enable_if_t<
                                    Enable && C == ConditionContextType::Free>>
  explicit Junction(JunctionOperator op, bool positive, const Problem *problem)
      : BaseJunction{op, positive}, ConditionContext<C>{problem} {}

  explicit Junction(JunctionOperator op, bool positive, Handle<Action> action)
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

struct Action {
  friend Problem;

  std::string name;
  std::vector<std::unique_ptr<Parameter>> parameters;
  std::shared_ptr<Precondition> precondition;
  std::shared_ptr<Effect> effect;
  const Problem *problem;
  size_t id;

  Handle<Parameter> add_parameter(std::string name, Handle<Type> type);
  void set_precondition(std::shared_ptr<Precondition> precondition);
  void set_effect(std::shared_ptr<Effect> effect);
  Handle<Parameter> get_parameter(const std::string &name) const;

private:
  Action(std::string name, const Problem *problem, size_t id)
      : name{std::move(name)}, problem{problem}, id{id} {}
};

class Problem {
public:
  void set_domain_name(std::string name);
  void set_problem_name(std::string name, const std::string &domain_ref);
  void add_requirement(std::string name) {
    requirements_.push_back(std::move(name));
  }
  Handle<Type> add_type(std::string name);
  Handle<Type> add_type(std::string name, Handle<Type> supertype);
  Handle<Constant> add_constant(std::string name, Handle<Type> type);
  Handle<Predicate> add_predicate(std::string name);
  void add_parameter_type(Handle<Predicate> predicate, Handle<Type> type);
  Handle<Action> add_action(std::string name);
  Handle<Parameter> add_parameter(Handle<Action> action, std::string name,
                                  Handle<Type> type);
  void set_precondition(Handle<Action> action,
                        std::shared_ptr<Precondition> precondition);
  void set_effect(Handle<Action> action, std::shared_ptr<Effect> effect);
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
  Handle<Type> get_type(const std::string &name) const;
  Handle<Constant> get_constant(const std::string &name) const;
  Handle<Predicate> get_predicate(const std::string &name) const;
  Handle<Action> get_action(const std::string &name) const;
  const auto &get_init() const { return init_; }
  const auto &get_goal() const { return goal_; }

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

#endif /* end of include guard: PROBLEM_HPP */
