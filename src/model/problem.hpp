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
  std::string name;
  const Type *supertype;
};

bool is_subtype(const Type *first, const Type *second);

class Predicate {
public:
  std::string name;
  explicit Predicate(std::string name, const Problem *problem)
      : name{std::move(name)}, problem_{problem} {}

  void add_parameter_type(Handle<Type> type);
  const auto &get_parameter_types() const { return parameter_types_; }

private:
  std::vector<const Type *> parameter_types_;
  const Problem *problem_;
};

struct Constant {
  std::string name;
  const Type *type;
};

struct Parameter {
  std::string name;
  const Type *type;
};

class Action;
enum class ConditionContext { Free, Precondition, Effect };

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

template <ConditionContext C> class Condition;
using FreeCondition = Condition<ConditionContext::Free>;
using Precondition = Condition<ConditionContext::Precondition>;
using Effect = Condition<ConditionContext::Effect>;

class Action {
public:
  std::string name;

  explicit Action(std::string name, const Problem *problem)
      : name{std::move(name)}, problem_{problem} {}

  Handle<Parameter> add_parameter(std::string name, Handle<Type> type);
  void set_precondition(std::unique_ptr<Precondition> precondition);
  void set_effect(std::unique_ptr<Effect> effect);
  Handle<Parameter> get_parameter(const std::string &name) const;
  const auto &get_parameters() const { return parameters_; }
  const Precondition *get_precondition() const { return precondition_.get(); }
  const Effect *get_effect() const { return effect_.get(); }

private:
  std::vector<std::unique_ptr<Parameter>> parameters_;
  std::unique_ptr<Precondition> precondition_;
  std::unique_ptr<Effect> effect_;
  const Problem *problem_;
};

template <ConditionContext C> class Condition {
public:
  explicit Condition(bool valid, bool positive, Handle<Action> action)
      : valid_{valid}, positive_{positive}, action_{action.get()},
        problem_{action.get_problem()} {}

  bool valid() const { return valid_; }
  bool positive() const { return positive_; }
  const Action *get_action() const { return action_; }
  const Problem *get_problem() const { return problem_; }

protected:
  bool valid_;
  bool positive_;
  const Action *action_;
  const Problem *problem_;
};

template <> class Condition<ConditionContext::Free> {
public:
  explicit Condition(bool valid, bool positive, const Problem *problem)
      : valid_{valid}, positive_{positive}, problem_{problem} {}

  bool valid() const { return valid_; }
  bool positive() const { return positive_; }
  const Problem *get_problem() const { return problem_; }

protected:
  bool valid_;
  bool positive_;
  const Problem *problem_;
};

using Argument = std::variant<const Parameter *, const Constant *>;

template <ConditionContext C> class AtomicCondition : public Condition<C> {
public:
  template <bool Enable = true,
            typename = std::enable_if_t<Enable && C == ConditionContext::Free>>
  explicit AtomicCondition(bool positive, Handle<Predicate> predicate)
      : Condition<C>{false, positive, predicate.get_problem()},
        predicate_{predicate.get()} {
    if (predicate->name == "=") {
      throw ModelException{"Predicate \"=\" can only be used in preconditions"};
    }
  }

  template <bool Enable = true,
            typename = std::enable_if_t<Enable && C != ConditionContext::Free>>
  explicit AtomicCondition(bool positive, Handle<Predicate> predicate,
                           Handle<Action> action)
      : Condition<C>{false, positive, action}, predicate_{predicate.get()} {
    if (predicate.get_problem() != action.get_problem()) {
      throw ModelException{"Predicate and action not from the same problem"};
    }
    if (predicate->name == "=" && C != ConditionContext::Precondition) {
      throw ModelException{"Predicate \"=\" can only be used in preconditions"};
    }
  }

  template <bool Enable = true,
            typename = std::enable_if_t<Enable && C != ConditionContext::Free>>
  AtomicCondition &add_bound_argument(Handle<Parameter> parameter) {
    if (Condition<C>::action_ != parameter.get_action()) {
      throw ModelException{"Parameter is not from the same action"};
    }
    if (arguments_.size() == predicate_->get_parameter_types().size()) {
      throw ModelException{
          "Number of arguments exceeded: Predicate \'" + predicate_->name +
          "\' takes " +
          std::to_string(predicate_->get_parameter_types().size()) +
          " arguments"};
    }
    if (!is_subtype(parameter->type,
                    predicate_->get_parameter_types()[arguments_.size()])) {
      throw ModelException{
          "Type mismatch of bound argument \'" + parameter->name +
          "\': Expected a subtype of \'" +
          predicate_->get_parameter_types()[arguments_.size()]->name +
          "\' but got type \'" + parameter->type->name + "\'"};
    }
    arguments_.push_back(std::move(parameter.get()));
    return *this;
  }

  AtomicCondition &add_constant_argument(Handle<Constant> constant) {
    if (Condition<C>::problem_ != constant.get_problem()) {
      throw ModelException{"Constant is not from same problem"};
    }
    if (arguments_.size() == predicate_->get_parameter_types().size()) {
      throw ModelException{
          "Number of arguments exceeded: Predicate \'" + predicate_->name +
          "\' takes " +
          std::to_string(predicate_->get_parameter_types().size()) +
          " arguments"};
    }
    if (!is_subtype(constant->type,
                    predicate_->get_parameter_types()[arguments_.size()])) {
      throw ModelException{
          "Type mismatch of constant argument \'" + constant->name +
          "\': Expected a subtype of \'" +
          predicate_->get_parameter_types()[arguments_.size()]->name +
          "\' but got type \'" + constant->type->name + "\'"};
    }
    arguments_.push_back(std::move(constant.get()));
    return *this;
  }

  void finish() {
    assert(arguments_.size() <= predicate_->get_parameter_types().size());
    if (arguments_.size() != predicate_->get_parameter_types().size()) {
      throw ModelException{
          "Too few arguments: Expected " +
          std::to_string(predicate_->get_parameter_types().size()) +
          " but got " + std::to_string(arguments_.size())};
    }
    Condition<C>::valid_ = true;
  }

  const auto &get_arguments() const { return arguments_; }

private:
  const Predicate *predicate_;
  std::vector<
      std::conditional_t<C == ConditionContext::Free, Constant, Argument>>
      arguments_;
};

enum class JunctionOperator { And, Or };

template <ConditionContext C> class Junction : public Condition<C> {
public:
  template <bool Enable = true,
            typename = std::enable_if_t<Enable && C == ConditionContext::Free>>
  explicit Junction(JunctionOperator op, bool positive, const Problem *problem)
      : Condition<C>{true, positive, problem}, op_{op} {}

  template <bool Enable = true,
            typename =
                std::enable_if_t<Enable && C == ConditionContext::Precondition>>
  explicit Junction(JunctionOperator op, bool positive, Handle<Action> action)
      : Condition<C>{true, positive, action}, op_{op} {}

  template <bool Enable = true, typename = std::enable_if_t<
                                    Enable && C == ConditionContext::Effect>>
  explicit Junction(Handle<Action> action)
      : Condition<C>{true, true, action}, op_{JunctionOperator::And} {}

  JunctionOperator get_operator() const { return op_; }

  void add_condition(std::unique_ptr<Condition<C>> condition) {
    if (Condition<C>::problem_ != condition->get_problem()) {
      throw ModelException{"Condition is not from the same problem"};
    }
    if constexpr (C != ConditionContext::Free) {
      if (Condition<C>::action_ != condition->get_action()) {
        throw ModelException{"Condition is not from the same action"};
      }
    }
    if (!condition->valid()) {
      throw ModelException{"Condition is not valid"};
    }
    conditions_.push_back(std::move(condition));
  }

  const auto &get_conditions() const { return conditions_; }

private:
  JunctionOperator op_;
  std::vector<std::unique_ptr<Condition<C>>> conditions_;
};

using InitPredicate = AtomicCondition<ConditionContext::Free>;
using GoalCondition = Condition<ConditionContext::Free>;

class Problem {
public:
  void set_domain_name(std::string name);
  void set_problem_name(std::string name, const std::string &domain_ref);
  void add_requirement(std::string name);
  Handle<Type> add_type(std::string name);
  Handle<Type> add_type(std::string name, Handle<Type> supertype);
  Handle<Constant> add_constant(std::string name, Handle<Type> type);
  Handle<Predicate> add_predicate(std::string name);
  void add_parameter_type(Handle<Predicate> predicate, Handle<Type> type);
  Handle<Action> add_action(std::string name);
  Handle<Parameter> add_parameter(Handle<Action> action, std::string name,
                                  Handle<Type> type);
  void set_precondition(Handle<Action> action,
                        std::unique_ptr<Precondition> precondition);
  void set_effect(Handle<Action> action, std::unique_ptr<Effect> effect);
  void add_init(std::unique_ptr<InitPredicate> init);
  void set_goal(std::unique_ptr<GoalCondition> goal);

  const std::string &get_domain_name() const { return domain_name_; }
  const std::string &get_problem_name() const { return problem_name_; }
  const std::vector<std::string> &get_requirements() const {
    return requirements_;
  }
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
  std::vector<std::unique_ptr<AtomicCondition<ConditionContext::Free>>> init_;
  std::unique_ptr<Condition<ConditionContext::Free>> goal_;
};

#endif /* end of include guard: PROBLEM_HPP */
