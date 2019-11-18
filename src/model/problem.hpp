#ifndef PROBLEM_HPP
#define PROBLEM_HPP

#include <exception>
#include <list>
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
  explicit Predicate(std::string name, const Problem *problem)
      : name_{std::move(name)}, problem_{problem} {}

  void add_parameter_type(const std::string &type);

  const std::vector<const Type *> &get_parameter_types() const;

private:
  std::string name_;
  std::vector<const Type *> parameter_types_;
  const Problem *problem_;
};

using Argument = std::variant<const Parameter *, const Constant *>;

class AtomicCondition {
public:
  explicit AtomicCondition(bool positive, const Predicate *predicate,
                           const Action *action, const Problem *problem)
      : positive_{positive},
        predicate_{predicate}, action_{action}, problem_{problem} {}

  explicit AtomicCondition(bool positive, const Predicate *predicate,
                           const Problem *problem)
      : AtomicCondition{positive, predicate, nullptr, problem} {}

  void add_bound_argument(const std::string &parameter);

  void add_constant_argument(const std::string &constant);

  void finish();

private:
  bool positive_;
  const Predicate *predicate_;
  const Action *action_;
  const Problem *problem_;
  std::vector<Argument> arguments_;
  bool finished_ = false;
};

class Conjunction;
class Disjunction;

using Condition = std::variant<AtomicCondition, Conjunction, Disjunction>;

class Conjunction {
public:
  explicit Conjunction(bool positive, const Problem *problem)
      : positive_{positive}, problem_{problem} {}

  AtomicCondition &add_atomic_condition(bool positive,
                                        const std::string &predicate);

  Conjunction &add_conjuction(bool positive);

  Disjunction &add_disjunction(bool positive);

private:
  void finish_prev();
  bool positive_ = true;
  const Problem *problem_;
  std::vector<Condition> conditions_;
};

class Disjunction {
public:
  explicit Disjunction(bool positive, const Problem *problem)
      : positive_{positive}, problem_{problem} {}

  AtomicCondition &add_atomic_condition(bool positive,
                                        const std::string &predicate);

  Conjunction &add_conjuction(bool positive);

  Disjunction &add_disjunction(bool positive);

private:
  void finish_prev();
  bool positive_ = true;
  const Problem *problem_;
  std::vector<Condition> conditions_;
};

class Action {
public:
  explicit Action(std::string name, const Problem *problem)
      : name_{std::move(name)}, precondition_{Conjunction{true, problem}},
        effect_{Conjunction{true, problem}}, problem_{problem} {}

  const Parameter &get_parameter(const std::string &name) const;

  void add_parameter(std::string name, const std::string &type);

  AtomicCondition &set_atomic_precondition(bool negated,
                                           const std::string &predicate);

  Conjunction &set_conjunctive_precondition(bool negated);

  Disjunction &set_disjunctive_precondition(bool negated);

  AtomicCondition &set_atomic_effect(bool negated,
                                     const std::string &predicate);

  Conjunction &set_conjunctive_effect(bool negated);

  Disjunction &set_disjunctive_effect(bool negated);

private:
  std::string name_;
  std::list<Parameter> parameters_;
  Condition precondition_;
  Condition effect_;
  const Problem *problem_;
};

class Problem {
public:
  void set_domain_name(std::string name);

  void set_problem_name(std::string name, const std::string &domain_ref);

  void add_requirement(std::string name);

  const Type &get_type(const std::string &name) const;

  void add_type(std::string name, const std::string &supertype);

  const Constant &get_constant(const std::string &name) const;

  void add_constant(std::string name, std::string type);

  const Predicate &get_predicate(const std::string &name) const;

  Predicate &add_predicate(std::string name);

  Action &add_action(std::string name);

private:
  std::string domain_name_ = "";
  std::string problem_name_ = "";
  std::vector<std::string> requirements_;
  std::list<Type> types_;
  std::list<Constant> constants_;
  std::list<Predicate> predicates_;
  std::vector<Action> actions_;
};

#endif /* end of include guard: PROBLEM_HPP */
