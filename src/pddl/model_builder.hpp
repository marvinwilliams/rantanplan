#ifndef MODEL_BUILDER_HPP
#define MODEL_BUILDER_HPP

#include "logging/logging.hpp"
#include "model/parsed/model.hpp"
#include "pddl/ast/ast.hpp"
#include "pddl/ast/visitor.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

extern logging::Logger parser_logger;

namespace pddl {

class ModelBuilder : public ast::Visitor<ModelBuilder> {
public:
  friend ast::Visitor<ModelBuilder>;

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

  std::unique_ptr<parsed::Problem> parse(const ast::AST &ast);

private:
  using Visitor<ModelBuilder>::traverse;
  using Visitor<ModelBuilder>::visit_begin;
  using Visitor<ModelBuilder>::visit_end;

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
  bool visit_begin(const ast::Predicate &predicate);
  bool visit_end(const ast::Predicate &);
  bool visit_begin(const ast::PredicateEvaluation &predicate);
  bool visit_begin(const ast::Conjunction &conjunction);
  bool visit_begin(const ast::Disjunction &disjunction);
  bool visit_end(const ast::Condition &condition);
  bool visit_begin(const ast::Requirement &requirement);

  State state_ = State::Header;
  bool positive_ = true;
  parsed::TypeHandle root_type_ = parsed::TypeHandle{};
  parsed::TypeHandle current_type_ = parsed::TypeHandle{};
  parsed::PredicateHandle current_predicate_ = parsed::PredicateHandle{};
  parsed::ActionHandle current_action_ = parsed::ActionHandle{};
  std::vector<std::shared_ptr<parsed::Condition>> condition_stack_;
  size_t num_requirements_ = 0;
  size_t num_types_ = 0;
  size_t num_constants_ = 0;
  size_t num_predicates_ = 0;
  size_t num_actions_ = 0;
  std::unique_ptr<parsed::Problem> problem_;
};

} // namespace pddl

#endif /* end of include guard: MODEL_BUILDER_HPP */
