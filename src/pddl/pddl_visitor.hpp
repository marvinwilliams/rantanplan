#ifndef PDDL_VISITOR_HPP
#define PDDL_VISITOR_HPP

#include "logging/logging.hpp"
#include "model/problem.hpp"
#include "pddl/ast.hpp"
#include "pddl/visitor.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace pddl {

class PddlAstParser : public ast::Visitor<PddlAstParser> {
public:
  friend ast::Visitor<PddlAstParser>;

  enum class State {
    Header,
    Requirements,
    Types,
    Constants,
    Predicates,
    Action,
    Precondition,
    Effect,
    Init,
    Goal
  };

  static inline logging::Logger logger{"Ast"};

  std::unique_ptr<Problem> parse(const AST &ast);

private:
  using Visitor<PddlAstParser>::traverse;
  using Visitor<PddlAstParser>::visit_begin;
  using Visitor<PddlAstParser>::visit_end;

  void reset();
  bool visit_begin(const ast::Domain &domain);
  bool visit_begin(const ast::Problem &problem);
  bool visit_begin(const ast::SingleTypeIdentifierList &list);
  bool visit_end(const ast::SingleTypeIdentifierList &);
  bool visit_begin(const ast::SingleTypeVariableList &list);
  bool visit_end(const ast::SingleTypeVariableList &);
  bool visit_begin(const ast::IdentifierList &list);
  bool visit_begin(const ast::VariableList &list);
  bool visit_begin(const ast::ArgumentList &list);
  bool visit_begin(const ast::RequirementsDef &);
  bool visit_begin(const ast::TypesDef &);
  bool visit_begin(const ast::ConstantsDef &);
  bool visit_begin(const ast::PredicatesDef &);
  bool visit_begin(const ast::ActionDef &action_def);
  bool visit_end(const ast::ActionDef &);
  bool visit_begin(const ast::ObjectsDef &);
  bool visit_begin(const ast::InitDef &);
  bool visit_begin(const ast::GoalDef &);
  bool visit_begin(const ast::Effect &);
  bool visit_begin(const ast::Precondition &);
  bool visit_begin(const ast::Negation &negation);
  bool visit_end(const ast::Negation &);
  bool visit_begin(const ast::Predicate &ast_predicate);
  bool visit_end(const ast::Predicate &);
  bool visit_begin(const ast::PredicateEvaluation &ast_predicate);
  bool visit_begin(const ast::Conjunction &conjunction);
  bool visit_begin(const ast::Disjunction &disjunction);
  bool visit_end(const ast::Condition &condition);
  bool visit_begin(const ast::Requirement &ast_requirement);

  State state_ = State::Header;
  bool positive_ = true;
  Handle<Type> root_type_ = Handle<Type>{};
  Handle<Type> current_type_ = Handle<Type>{};
  Handle<Predicate> current_predicate_ = Handle<Predicate>{};
  Handle<Action> current_action_ = Handle<Action>{};
  std::vector<std::shared_ptr<Condition>> condition_stack_;
  size_t num_requirements_ = 0;
  size_t num_types_ = 0;
  size_t num_constants_ = 0;
  size_t num_predicates_ = 0;
  size_t num_actions_ = 0;
  std::unique_ptr<Problem> problem_;
};

} // namespace pddl

#endif /* end of include guard: PDDL_VISITOR_HPP */
