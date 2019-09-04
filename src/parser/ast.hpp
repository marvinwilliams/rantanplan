#ifndef AST_HPP
#define AST_HPP

#include "lexer/location.hpp"

#include <memory>
#include <variant>
#include <vector>

namespace parser {

namespace ast {

// The basic node for the ast contains only a lexer::Location
struct Node {
  lexer::Location location;
  virtual ~Node() {}

protected:
  Node(const lexer::Location &location) : location{location} {}
};

struct Identifier : Node {
  Identifier(const lexer::Location &location, const std::string &name)
      : Node{location}, name{name} {}

  std::string name;
};

struct Variable : Node {
  Variable(const lexer::Location &location, const std::string &name)
      : Node{location}, name{name} {}

  std::string name;
};

using Argument =
    std::variant<std::unique_ptr<Identifier>, std::unique_ptr<Variable>>;

struct Requirement : Node {
  Requirement(const lexer::Location &location, const std::string &name)
      : Node{location}, name{name} {}

  std::string name;
};

namespace detail {

template <typename T> struct List : Node {
  using value_type = T;

  List(const lexer::Location &location,
       std::unique_ptr<std::vector<value_type>> elements)
      : Node{location}, elements{std::move(elements)} {}

  std::unique_ptr<std::vector<value_type>> elements;
};

template <typename T> struct SingleTypeList : Node {
  using value_type = List<T>;

  SingleTypeList(const lexer::Location &location,
                 std::unique_ptr<value_type> list,
                 std::unique_ptr<Identifier> type = nullptr)
      : Node{location}, list{std::move(list)}, type{std::move(type)} {}

  std::unique_ptr<value_type> list;
  std::unique_ptr<Identifier> type;
};

} // namespace detail

using IdentifierList = detail::List<std::unique_ptr<Identifier>>;
using VariableList = detail::List<std::unique_ptr<Variable>>;
using RequirementList = detail::List<std::unique_ptr<Requirement>>;
using ArgumentList = detail::List<Argument>;
using SingleTypeIdentifierList =
    detail::SingleTypeList<std::unique_ptr<Identifier>>;
using SingleTypeVariableList =
    detail::SingleTypeList<std::unique_ptr<Variable>>;
using TypedIdentifierList =
    detail::List<std::unique_ptr<SingleTypeIdentifierList>>;
using TypedVariableList = detail::List<std::unique_ptr<SingleTypeVariableList>>;

struct Predicate : Node {
  Predicate(const lexer::Location &location, std::unique_ptr<Identifier> name,
            std::unique_ptr<TypedVariableList> parameters)
      : Node{location}, name{std::move(name)}, parameters{
                                                   std::move(parameters)} {}

  std::unique_ptr<Identifier> name;
  std::unique_ptr<TypedVariableList> parameters;
};

using PredicateList = detail::List<std::unique_ptr<Predicate>>;

struct PredicateEvaluation;
struct Conjunction;
struct Disjunction;
struct Negation;
struct Imply;
struct Quantification;
struct When;
struct Empty;

using Condition =
    std::variant<std::unique_ptr<Empty>, std::unique_ptr<PredicateEvaluation>,
                 std::unique_ptr<Conjunction>, std::unique_ptr<Disjunction>,
                 std::unique_ptr<Negation>, std::unique_ptr<Imply>,
                 std::unique_ptr<Quantification>, std::unique_ptr<When>>;

using ConditionList = detail::List<Condition>;

struct PredicateEvaluation : Node {
  PredicateEvaluation(const lexer::Location &location,
                      std::unique_ptr<Identifier> name,
                      std::unique_ptr<ArgumentList> arguments)
      : Node{location}, name{std::move(name)}, arguments{std::move(arguments)} {
  }

  std::unique_ptr<Identifier> name;
  std::unique_ptr<ArgumentList> arguments;
};

struct Conjunction : Node {
  Conjunction(const lexer::Location &location,
              std::unique_ptr<ConditionList> conditions)
      : Node{location}, conditions{std::move(conditions)} {}

  std::unique_ptr<ConditionList> conditions;
};

struct Disjunction : Node {
  Disjunction(const lexer::Location &location,
              std::unique_ptr<ConditionList> conditions)
      : Node{location}, conditions{std::move(conditions)} {}

  std::unique_ptr<ConditionList> conditions;
};

struct Negation : Node {
  Negation(const lexer::Location &location, Condition condition)
      : Node{location}, condition{std::move(condition)} {}

  Condition condition;
};

struct Imply {};
struct Quantification {
  enum class Quantor { Exists, ForAll };
};
struct When {};

struct Empty {};

struct Precondition : Node {
  Precondition(const lexer::Location &location, Condition precondition)
      : Node{location}, precondition{std::move(precondition)} {}

  Condition precondition;
};

struct Effect : Node {
  Effect(const lexer::Location &location, Condition effect)
      : Node{location}, effect{std::move(effect)} {}

  Condition effect;
};

struct RequirementsDef : Node {
  RequirementsDef(const lexer::Location &location,
                  std::unique_ptr<RequirementList> requirement_list)
      : Node{location}, requirement_list{std::move(requirement_list)} {}

  std::unique_ptr<RequirementList> requirement_list;
};

struct TypesDef : Node {
  TypesDef(const lexer::Location &location,
           std::unique_ptr<TypedIdentifierList> type_list)
      : Node{location}, type_list{std::move(type_list)} {}

  std::unique_ptr<TypedIdentifierList> type_list;
};

struct ConstantsDef : Node {
  ConstantsDef(const lexer::Location &location,
               std::unique_ptr<TypedIdentifierList> constant_list)
      : Node{location}, constant_list{std::move(constant_list)} {}

  std::unique_ptr<TypedIdentifierList> constant_list;
};

struct PredicatesDef : Node {
  PredicatesDef(const lexer::Location &location,
                std::unique_ptr<PredicateList> predicate_list)
      : Node{location}, predicate_list{std::move(predicate_list)} {}

  std::unique_ptr<PredicateList> predicate_list;
};

struct ActionDef : Node {
  ActionDef(const lexer::Location &location, std::unique_ptr<Identifier> name,
            std::unique_ptr<TypedVariableList> parameters,
            std::unique_ptr<Precondition> precondition = nullptr,
            std::unique_ptr<Effect> effect = nullptr)
      : Node{location}, name{std::move(name)}, parameters{std::move(
                                                   parameters)},
        precondition{std::move(precondition)}, effect{std::move(effect)} {}

  std::unique_ptr<Identifier> name;
  std::unique_ptr<TypedVariableList> parameters;
  std::unique_ptr<Precondition> precondition;
  std::unique_ptr<Effect> effect;
};

struct ObjectsDef : Node {
  ObjectsDef(const lexer::Location &location,
             std::unique_ptr<TypedIdentifierList> objects)
      : Node{location}, objects{std::move(objects)} {}

  std::unique_ptr<TypedIdentifierList> objects;
};

struct InitDef : Node {
  InitDef(const lexer::Location &location, Condition init_condition)
      : Node{location}, init_condition{std::move(init_condition)} {}

  Condition init_condition;
};

struct GoalDef : Node {
  GoalDef(const lexer::Location &location, Condition goal)
      : Node{location}, goal{std::move(goal)} {}

  Condition goal;
};

using Element =
    std::variant<std::unique_ptr<RequirementsDef>, std::unique_ptr<TypesDef>,
                 std::unique_ptr<ConstantsDef>, std::unique_ptr<PredicatesDef>,
                 std::unique_ptr<ActionDef>, std::unique_ptr<ObjectsDef>,
                 std::unique_ptr<InitDef>, std::unique_ptr<GoalDef>>;
using ElementList = detail::List<Element>;

struct Domain : Node {
  Domain(const lexer::Location &location, std::unique_ptr<Identifier> name,
         std::unique_ptr<ElementList> domain_body)
      : Node{location}, name{std::move(name)}, domain_body{
                                                   std::move(domain_body)} {}

  std::unique_ptr<Identifier> name;
  std::unique_ptr<ElementList> domain_body;
};

struct Problem : Node {
  Problem(const lexer::Location &location, std::unique_ptr<Identifier> name,
          std::unique_ptr<Identifier> domain_ref,
          std::unique_ptr<ElementList> problem_body)
      : Node{location}, name{std::move(name)},
        domain_ref{std::move(domain_ref)}, problem_body{
                                               std::move(problem_body)} {}

  std::unique_ptr<Identifier> name;
  std::unique_ptr<Identifier> domain_ref;
  std::unique_ptr<ElementList> problem_body;
};

/* The AST is built while parsing. It abstracts the entire input and can later
 * be traversed by a visitor. Most constructors only take unique pointers so
 * that the AST must be built inplace */
class AST {
public:
  AST() {}

  void set_domain(std::unique_ptr<Domain> domain) {
    domain_ = std::move(domain);
  }

  void set_problem(std::unique_ptr<Problem> problem) {
    problem_ = std::move(problem);
  }

  const Domain *get_domain() const { return domain_.get(); }
  const Problem *get_problem() const { return problem_.get(); }

private:
  std::unique_ptr<Domain> domain_;
  std::unique_ptr<Problem> problem_;
};

} // namespace ast

} // namespace parser

#endif /* end of include guard: AST_HPP */
