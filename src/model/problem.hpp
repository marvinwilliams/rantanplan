#ifndef PROBLEM_HPP
#define PROBLEM_HPP

#include <exception>
#include <list>
#include <string>
#include <variant>
#include <vector>

struct ModelException : public std::exception {
  explicit ModelException(std::string message) : message{std::move(message)} {}

  [[nodiscard]] inline const char *what() const noexcept override {
    return message.c_str();
  }

  const std::string message;
};

class Problem;
class Action;

struct Type {
  std::string name;
  const Type *supertype;
};

bool is_subtype(const Type *subtype, const Type *supertype);

struct Constant {
  std::string name;
  const Type *type;
};

struct Parameter {
  std::string name;
  const Type *type;
};

class Predicate {
public:
  const std::string name;
  const Problem *const problem;

  explicit Predicate(std::string name, const Problem *problem)
      : name{std::move(name)}, problem{problem} {}

  Predicate &add_parameter_type(const std::string &type);
  const auto &get_parameter_types() const;

private:
  std::vector<const Type *> parameter_types_;
};

enum class ConditionContext { Init, Goal, Precondition, Effect };

class Junction;
class AtomicCondition;

using Argument = std::variant<const Parameter *, const Constant *>;

using Condition = std::variant<AtomicCondition, Junction>;

class Junction {
public:
  enum class Operator { And, Or };

  const Operator op;
  const bool positive;
  const ConditionContext context;
  const Action *const action;
  const Problem *const problem;

  explicit Junction(Operator op, bool positive, ConditionContext context,
                    const Action *action, const Problem *problem);

  explicit Junction(Operator op, bool positive, const Junction &parent)
      : Junction{op, positive, parent.context, parent.action, parent.problem} {}

  explicit Junction(Operator op, bool positive, ConditionContext context,
                    const Problem *problem)
      : Junction{op, positive, context, nullptr, problem} {}

  Junction &add_condition(Condition condition);
  const auto &get_conditions() const;

private:
  std::vector<Condition> conditions_;
};

class AtomicCondition {
public:
  const bool positive;
  const ConditionContext context;
  const Predicate *const predicate;
  const Action *const action;
  const Problem *const problem;

  explicit AtomicCondition(bool positive, const Predicate *predicate,
                           ConditionContext context, const Action *action,
                           const Problem *problem);

  explicit AtomicCondition(bool positive, const Predicate *predicate,
                           const Junction &parent)
      : AtomicCondition{positive, predicate, parent.context, parent.action,
                        parent.problem} {}

  explicit AtomicCondition(bool positive, const Predicate *predicate,
                           ConditionContext context, const Problem *problem)
      : AtomicCondition{positive, predicate, context, nullptr, problem} {}

  AtomicCondition &add_bound_argument(const std::string &parameter);
  AtomicCondition &add_constant_argument(const std::string &constant);
  const auto &get_arguments() const;
  void check_complete() const;

private:
  std::vector<Argument> arguments_;
};

class Action {
public:
  const std::string name;
  const Problem *const problem;

  explicit Action(std::string name, const Problem *problem)
      : name{std::move(name)}, problem{problem},
        precondition_{Junction{Junction::Operator::And, true,
                               ConditionContext::Precondition, this, problem}},
        effect_{Junction{Junction::Operator::And, true,
                         ConditionContext::Effect, this, problem}} {}

  const Parameter &get_parameter(const std::string &name) const;
  Action &add_parameter(std::string name, const std::string &type);
  void set_precondition(Condition precondition);
  void set_effect(Condition effect);
  const auto &get_parameters() const;
  const Condition &get_precondition() const;
  const Condition &get_effect() const;

private:
  std::list<Parameter> parameters_;
  Condition precondition_;
  Condition effect_;
};

class Problem {
public:
  explicit Problem()
      : goal_{Junction{Junction::Operator::And, true, ConditionContext::Goal,
                       this}} {}
  void set_domain_name(std::string name);
  void set_problem_name(std::string name, const std::string &domain_ref);
  Problem &add_requirement(std::string name);
  const Type &get_type(const std::string &name) const;
  const Type &get_root_type() const;
  Problem &add_type(std::string name, const std::string &supertype);
  const Constant &get_constant(const std::string &name) const;
  Problem &add_constant(std::string name, std::string type);
  const Predicate &get_predicate(const std::string &name) const;
  Problem &add_predicate(Predicate predicate);
  Problem &add_action(Action action);
  void set_init(Condition condition);
  void set_goal(Condition condition);

private:
  std::string domain_name_ = "";
  std::string problem_name_ = "";
  std::vector<std::string> requirements_;
  std::list<Type> types_ = {{"_root", nullptr}};
  std::list<Constant> constants_;
  std::list<Predicate> predicates_;
  std::vector<Action> actions_;
  std::vector<AtomicCondition> init_;
  Condition goal_;
};

#endif /* end of include guard: PROBLEM_HPP */
