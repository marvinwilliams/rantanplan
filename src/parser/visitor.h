#ifndef VISITOR_H
#define VISITOR_H

#include "ast.h"
#include <type_traits>

namespace parser {

namespace visitor {

using namespace ast;

/*This CRTP-Visitor provides the functionality to traverse the AST. Ther
 * traverse(*) method recursively traverses the AST and calls
 * visit_begin(*)/visit_end(*) on each element it encounters. The boolean return
 * value of both methods indicates wether to continue traversing. You can
 * provide your own traverse(*) and visit_begin(*)/visit_end(*) implementations
 * in the Derived class */
template <typename Derived> class Visitor {
public:
  Visitor() : variant_visitor_{get_derived_()} {}

  bool traverse(const AST &ast) {
    get_derived_().visit_begin(ast);
    if (!ast.get_domain()) {
      return get_derived_().visit_end(ast);
    }
    return get_derived_().traverse(*ast.get_domain()) &&
           get_derived_().visit_end(ast);
  }

  bool traverse(const Domain &domain) {
    return get_derived_().visit_begin(domain) &&
           get_derived_().traverse(*domain.name) &&
    traverse_(*domain.domain_body) && get_derived_().visit_end(domain);
  }

  bool traverse(const Element &element) {
    return get_derived_().visit_begin(element) &&
           std::visit(variant_visitor_, element) && visit_end(element);
  }

  bool traverse(const RequirementsDef &requirements_def) {
    return get_derived_().visit_begin(requirements_def) &&
           get_derived_().traverse(*requirements_def.requirements) &&
           get_derived_().visit_end(requirements_def);
  }

  bool traverse(const TypesDef &types_def) {
    return get_derived_().visit_begin(types_def) &&
           get_derived_().traverse(*types_def.type_list) &&
           get_derived_().visit_end(types_def);
  }

  bool traverse(const ConstantsDef &constants_def) {
    return get_derived_().visit_begin(constants_def) &&
           get_derived_().traverse(*constants_def.constant_list) &&
           get_derived_().visit_end(constants_def);
  }

  bool traverse(const PredicatesDef &predicates_def) {
    return get_derived_().visit_begin(predicates_def) &&
           get_derived_().traverse(*predicates_def.predicate_list) &&
           get_derived_().visit_end(predicates_def);
  }

  bool traverse(const ActionDef &action_def) {
    return get_derived_().visit_begin(action_def) &&
           get_derived_().traverse(*action_def.name) &&
           get_derived_().traverse(*action_def.parameters) &&
           (action_def.precondition
                ? get_derived_().traverse(*action_def.precondition.value())
                : true) &&
           (action_def.effect
                ? get_derived_().traverse(*action_def.effect.value())
                : true) &&
           get_derived_().visit_end(action_def);
  }

  bool traverse(const Precondition &precondition) {
    return get_derived_().visit_begin(precondition) &&
           get_derived_().traverse(*precondition.precondition) &&
           get_derived_().visit_end(precondition);
  }

  bool traverse(const Effect &effect) {
    return get_derived_().visit_begin(effect) &&
           get_derived_().traverse(*effect.effect) &&
           get_derived_().visit_end(effect);
  }

  bool traverse(const Predicate &predicate) {
    return get_derived_().visit_begin(predicate) &&
           get_derived_().traverse(*predicate.name) &&
           get_derived_().traverse(*predicate.parameters) &&
           get_derived_().visit_end(predicate);
  }

  bool traverse(const Condition &condition) {
    return get_derived_().visit_begin(condition) &&
           std::visit(variant_visitor_, condition);
    get_derived_().visit_end(condition);
  }

  bool traverse(const PredicateEvaluation &predicate_evaluation) {
    return get_derived_().visit_begin(predicate_evaluation) &&
           get_derived_().traverse(*predicate_evaluation.name) &&
           get_derived_().traverse(*predicate_evaluation.arguments) &&
           get_derived_().visit_end(predicate_evaluation);
  }

  bool traverse(const Conjunction &conjunction) {
    return get_derived_().visit_begin(conjunction) &&
           get_derived_().traverse(*conjunction.conditions) &&
           get_derived_().visit_end(conjunction);
  }

  bool traverse(const Disjunction &disjunction) {
    return get_derived_().visit_begin(disjunction) &&
           get_derived_().traverse(*disjunction.conditions) &&
           get_derived_().visit_end(disjunction);
  }

  bool traverse(const Negation &negation) {
    return get_derived_().visit_begin(negation) &&
           get_derived_().traverse(*negation.condition) &&
           get_derived_().visit_end(negation);
  }

  bool traverse(const Argument &argument) {
    return get_derived_().visit_begin(argument) &&
           std::visit(variant_visitor_, argument);
    get_derived_().visit_end(argument);
  }

  template <typename ListElement>
  bool traverse(const detail::List<ListElement> &list) {
    return get_derived_().visit_begin(list) && traverse_(*list.elements) &&
           get_derived_().visit_end(list);
  }

  template <typename ListElement>
  bool traverse(const detail::SingleTypeList<ListElement> &single_typed_list) {
    return get_derived_().visit_begin(single_typed_list) &&
           get_derived_().traverse(*single_typed_list.list) &&
           (single_typed_list.type
                ? get_derived_().traverse(single_typed_list.type.value())
                : true) &&
           get_derived_().visit_end(single_typed_list);
  }

  template <typename ListElement>
  bool traverse(const detail::TypedList<ListElement> &typed_list) {
    return get_derived_().visit_begin(typed_list) &&
           traverse_(*typed_list.lists) && get_derived_().visit_end(typed_list);
  }

  template <typename Node> bool traverse(const Node &node) {
    return get_derived_().visit_begin(node) && get_derived_().visit_end(node);
  }

  // Visit functions to be overwritten.
  template <typename Node> bool visit_begin(const Node &) { return true; }
  template <typename Node> bool visit_end(const Node &) { return true; }

  virtual ~Visitor() {}

private:
  Derived &get_derived_() { return *static_cast<Derived *>(this); }

  struct VariantVisitor {
    VariantVisitor(Derived &derived) : derived{derived} {}

    bool operator()(const std::monostate &) { return true; }
    template <typename Node> bool operator()(const Node &node) {
      return derived.traverse(node);
    }

    Derived &derived;
  };

  template <typename T>
  bool traverse_(const std::vector<std::unique_ptr<T>> &element) {
    for (auto &c : element) {
      if (!get_derived_().traverse(*c)) {
        return false;
      }
    }
    return true;
  }

  VariantVisitor variant_visitor_;
};

} // namespace visitor
} // namespace parser

#endif /* end of include guard: VISITOR_H */
