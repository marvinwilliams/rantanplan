#ifndef PDDL_VISITOR_HPP
#define PDDL_VISITOR_HPP

#include "logging/logging.hpp"
#include "model/model.hpp"
#include "pddl/visitor.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace pddl {

class PddlAstParser : public ast::Visitor<PddlAstParser> {
public:
  friend ast::Visitor<PddlAstParser>;

  struct Context {
    enum class State {
      Requirements,
      Types,
      Constants,
      Predicates,
      Actions,
      Precondition,
      Effect,
      InitialState,
      Goal
    };

    bool negated = false;
    model::TypeHandle type_handle{0};
    std::unordered_map<std::string, model::ParameterHandle> parameter_lookup;
    std::unordered_map<std::string, model::TypeHandle> type_lookup;
    std::unordered_map<std::string, model::ConstantHandle> constant_lookup;
    std::unordered_map<std::string, model::PredicateHandle> predicate_lookup;
    std::vector<model::Condition> condition_stack_;
    State state;
  };

  using State = Context::State;

  static logging::Logger logger;

  std::unique_ptr<model::AbstractProblem> parse(const AST &ast);

private:
  using Visitor<PddlAstParser>::traverse;
  using Visitor<PddlAstParser>::visit_begin;
  using Visitor<PddlAstParser>::visit_end;

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
  bool visit_begin(const ast::ObjectsDef &);
  bool visit_begin(const ast::InitDef &);
  bool visit_begin(const ast::GoalDef &);
  bool visit_begin(const ast::Effect &);
  bool visit_begin(const ast::Precondition &);
  bool visit_begin(const ast::Negation &negation);
  bool visit_end(const ast::Negation &);
  bool visit_begin(const ast::Predicate &ast_predicate);
  bool visit_begin(const ast::PredicateEvaluation &ast_predicate);
  bool visit_begin(const ast::Conjunction &conjunction);
  bool visit_begin(const ast::Disjunction &disjunction);
  bool visit_end(const ast::Condition &condition);
  bool visit_begin(const ast::Requirement &ast_requirement);

  Context context_;
  model::AbstractProblem *problem_;
  model::ProblemHeader header_;
};

} // namespace pddl

#endif /* end of include guard: PDDL_VISITOR_HPP */
