#ifndef AST_HPP
#define AST_HPP

#include "lexer/location.hpp"

#include <memory>
#include <optional>
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

struct Name : Node {
  Name(const lexer::Location &location, const std::string &name)
      : Node{location}, name{name} {}

  std::string name;
};

struct Variable : Node {
  Variable(const lexer::Location &location, const std::string &variable)
      : Node{location}, variable{variable} {}

  std::string variable;
};

using Argument = std::variant<Name, Variable>;

struct Requirement : Node {
  Requirement(const lexer::Location &location, const std::string &name)
      : Node{location}, name{name} {}

  std::string name;
};

namespace detail {

template <typename T> struct List : Node {
  using value_type = T;

  List(const lexer::Location &location,
       std::unique_ptr<std::vector<std::unique_ptr<value_type>>> elements)
      : Node{location}, elements{std::move(elements)} {}

  std::unique_ptr<std::vector<std::unique_ptr<value_type>>> elements;
};

template <typename T> struct SingleTypeList : Node {
  using element_type = List<T>;

  SingleTypeList(const lexer::Location &location,
                 std::unique_ptr<element_type> list,
                 std::optional<std::unique_ptr<Name>> type = std::nullopt)
      : Node{location}, list{std::move(list)}, type{std::move(type)} {}

  std::unique_ptr<element_type> list;
  std::optional<std::unique_ptr<Name>> type;
};

template <typename T> struct TypedList : Node {
  using value_type = SingleTypeList<T>;

  TypedList(const lexer::Location &location,
            std::unique_ptr<std::vector<std::unique_ptr<value_type>>> lists)
      : Node{location}, lists{std::move(lists)} {}

  std::unique_ptr<std::vector<std::unique_ptr<value_type>>> lists;
};

} // namespace detail

using NameList = detail::List<Name>;
using VariableList = detail::List<Variable>;
using RequirementList = detail::List<Requirement>;
using ArgumentList = detail::List<Argument>;
using SingleTypedNameList = detail::SingleTypeList<Name>;
using SingleTypedVariableList = detail::SingleTypeList<Variable>;
using TypedNameList = detail::TypedList<Name>;
using TypedVariableList = detail::TypedList<Variable>;

struct RequirementsDef : Node {
  RequirementsDef(const lexer::Location &location,
                  std::unique_ptr<RequirementList> requirement_list)
      : Node{location}, requirement_list{std::move(requirement_list)} {}

  std::unique_ptr<RequirementList> requirement_list;
};

struct TypesDef : Node {
  TypesDef(const lexer::Location &location,
           std::unique_ptr<TypedNameList> type_list)
      : Node{location}, type_list{std::move(type_list)} {}

  std::unique_ptr<TypedNameList> type_list;
};

struct ConstantsDef : Node {
  ConstantsDef(const lexer::Location &location,
               std::unique_ptr<TypedNameList> constant_list)
      : Node{location}, constant_list{std::move(constant_list)} {}

  std::unique_ptr<TypedNameList> constant_list;
};

struct Predicate : Node {
  Predicate(const lexer::Location &location, std::unique_ptr<Name> name,
            std::unique_ptr<TypedVariableList> parameters)
      : Node{location}, name{std::move(name)}, parameters{
                                                   std::move(parameters)} {}

  std::unique_ptr<Name> name;
  std::unique_ptr<TypedVariableList> parameters;
};

using PredicateList = detail::List<Predicate>;

struct PredicatesDef : Node {
  PredicatesDef(const lexer::Location &location,
                std::unique_ptr<PredicateList> predicate_list)
      : Node{location}, predicate_list{std::move(predicate_list)} {}

  std::unique_ptr<PredicateList> predicate_list;
};

struct PredicateEvaluation;
struct Conjunction;
struct Disjunction;
struct Negation;

using Condition = std::variant<std::monostate, PredicateEvaluation, Conjunction,
                               Disjunction, Negation>;

using ConditionList = detail::List<Condition>;

struct PredicateEvaluation : Node {
  PredicateEvaluation(const lexer::Location &location,
                      std::unique_ptr<Name> name,
                      std::unique_ptr<ArgumentList> arguments)
      : Node{location}, name{std::move(name)}, arguments{std::move(arguments)} {
  }

  std::unique_ptr<Name> name;
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
  Negation(const lexer::Location &location,
           std::unique_ptr<Condition> condition)
      : Node{location}, condition{std::move(condition)} {}

  std::unique_ptr<Condition> condition;
};

struct Precondition : Node {
  Precondition(const lexer::Location &location,
               std::unique_ptr<Condition> precondition)
      : Node{location}, precondition{std::move(precondition)} {}

  std::unique_ptr<Condition> precondition;
};

struct Effect : Node {
  Effect(const lexer::Location &location, std::unique_ptr<Condition> effect)
      : Node{location}, effect{std::move(effect)} {}

  std::unique_ptr<Condition> effect;
};

struct ActionDef : Node {
  ActionDef(const lexer::Location &location, std::unique_ptr<Name> name,
            std::unique_ptr<TypedVariableList> parameters,
            std::optional<std::unique_ptr<Precondition>> precondition,
            std::optional<std::unique_ptr<Effect>> effect)
      : Node{location}, name{std::move(name)}, parameters{std::move(
                                                   parameters)},
        precondition{std::move(precondition)}, effect{std::move(effect)} {}

  std::unique_ptr<Name> name;
  std::unique_ptr<TypedVariableList> parameters;
  std::optional<std::unique_ptr<Precondition>> precondition;
  std::optional<std::unique_ptr<Effect>> effect;
};

struct ObjectsDef : Node {
  ObjectsDef(const lexer::Location &location,
             std::unique_ptr<TypedNameList> objects)
      : Node{location}, objects{std::move(objects)} {}

  std::unique_ptr<TypedNameList> objects;
};

struct InitPredicate : Node {
  InitPredicate(const lexer::Location &location, std::unique_ptr<Name> name,
                std::unique_ptr<NameList> arguments)
      : Node{location}, name{std::move(name)}, arguments{std::move(arguments)} {
  }

  std::unique_ptr<Name> name;
  std::unique_ptr<NameList> arguments;
};

struct InitNegation;
using InitCondition = std::variant<InitPredicate, InitNegation>;

struct InitNegation : Node {
  InitNegation(const lexer::Location &location,
               std::unique_ptr<InitCondition> init_condition)
      : Node{location}, init_condition{std::move(init_condition)} {}

  std::unique_ptr<InitCondition> init_condition;
};

using InitList = detail::List<InitCondition>;

struct InitDef : Node {
  InitDef(const lexer::Location &location,
          std::unique_ptr<InitList> init_predicates)
      : Node{location}, init_predicates{std::move(init_predicates)} {}

  std::unique_ptr<InitList> init_predicates;
};

struct GoalDef : Node {
  GoalDef(const lexer::Location &location, std::unique_ptr<Condition> goal)
      : Node{location}, goal{std::move(goal)} {}

  std::unique_ptr<Condition> goal;
};

using Element =
    std::variant<RequirementsDef, TypesDef, ConstantsDef, PredicatesDef,
                 ActionDef, ObjectsDef, InitDef, GoalDef>;
using ElementList = detail::List<Element>;

struct Domain : Node {
  Domain(const lexer::Location &location, std::unique_ptr<Name> name,
         std::unique_ptr<ElementList> domain_body)
      : Node{location}, name{std::move(name)}, domain_body{
                                                   std::move(domain_body)} {}

  std::unique_ptr<Name> name;
  std::unique_ptr<ElementList> domain_body;
};

struct Problem : Node {
  Problem(const lexer::Location &location, std::unique_ptr<Name> name,
          std::unique_ptr<Name> domain_ref,
          std::unique_ptr<ElementList> problem_body)
      : Node{location}, name{std::move(name)},
        domain_ref{std::move(domain_ref)}, problem_body{
                                               std::move(problem_body)} {}

  std::unique_ptr<Name> name;
  std::unique_ptr<Name> domain_ref;
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
