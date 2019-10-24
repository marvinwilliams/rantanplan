#ifndef PARSER_HPP
#define PARSER_HPP

#include "lexer/lexer.hpp"
#include "pddl/ast.hpp"
#include "pddl/parser_exception.hpp"
#include "pddl/tokens.hpp"
#include "logging/logging.hpp"

#include <memory>
#include <string>
#include <vector>

namespace pddl {

class Parser {

public:
  using Lexer = lexer::Lexer<pddl::TokenSet, pddl::TokenAction>;

  static logging::Logger logger;

  AST parse(const std::string &domain, const std::string &problem);

private:
  template <typename TokenType> void expect() {
    if (!lexer_.has_type<TokenType>()) {
      std::string msg = "Expected token \'" +
                        std::string(TokenType::printable_name) +
                        "\' but got \'" + lexer_.to_string() + '\'';
      throw ParserException(lexer_.location(), msg);
    }
  }

  template <typename TokenType> void skip() {
    expect<TokenType>();
    lexer_.next();
  }

  template <typename TokenType>[[nodiscard]] bool skip_if() {
    if (lexer_.has_type<TokenType>()) {
      lexer_.next();
      return true;
    }
    return false;
  }

  template <bool is_domain> ast::Element parse_element() {
    if (lexer_.has_type<token::Requirements>()) {
      return parse_requirements();
    } else if (is_domain && lexer_.has_type<token::Types>()) {
      return parse_types();
    } else if (is_domain && lexer_.has_type<token::Constants>()) {
      return parse_constants();
    } else if (is_domain && lexer_.has_type<token::Predicates>()) {
      return parse_predicates();
    } else if (lexer_.has_type<token::Functions>()) {
      return parse_functions();
    } else if (is_domain && lexer_.has_type<token::Action>()) {
      return parse_action();
    } else if (!is_domain && lexer_.has_type<token::Objects>()) {
      return parse_objects();
    } else if (!is_domain && lexer_.has_type<token::Init>()) {
      return parse_init();
    } else if (!is_domain && lexer_.has_type<token::Goal>()) {
      return parse_goal();
    } else if (lexer_.has_type<token::Metric>()) {
      return parse_metric();
    } else {
      expect<token::Requirement>();
      throw ParserException(
          lexer_.location(),
          "Unknown section: \'" +
              std::string{lexer_.get<token::Requirement>().name} + "\'");
    }
  }

  template <bool is_domain> std::unique_ptr<ast::ElementList> parse_elements() {
    const auto begin = lexer_.location();
    auto elements = std::make_unique<std::vector<ast::Element>>();
    while (skip_if<token::LParen>()) {
      skip_comments();
      elements->push_back(parse_element<is_domain>());
      skip<token::RParen>();
      skip_comments();
    }
    const auto &end = lexer_.location();
    return std::make_unique<ast::ElementList>(begin + end, std::move(elements));
  }

  void skip_comments();
  void advance();
  std::unique_ptr<ast::IdentifierList> parse_identifier_list();
  std::unique_ptr<ast::VariableList> parse_variable_list();
  std::unique_ptr<ast::ArgumentList> parse_argument_list();
  std::unique_ptr<ast::SingleTypeIdentifierList>
  parse_single_type_identifier_list();
  std::unique_ptr<ast::SingleTypeVariableList>
  parse_single_type_variable_list();
  std::unique_ptr<ast::TypedIdentifierList> parse_typed_identifier_list();
  std::unique_ptr<ast::TypedVariableList> parse_typed_variable_list();
  std::unique_ptr<ast::RequirementList> parse_requirement_list();
  std::unique_ptr<ast::RequirementsDef> parse_requirements();
  std::unique_ptr<ast::TypesDef> parse_types();
  std::unique_ptr<ast::ConstantsDef> parse_constants();
  std::unique_ptr<ast::Predicate> parse_predicate();
  std::unique_ptr<ast::PredicateList> parse_predicate_list();
  std::unique_ptr<ast::PredicatesDef> parse_predicates();
  std::unique_ptr<ast::PredicateEvaluation> parse_predicate_evaluation();
  std::unique_ptr<ast::Conjunction> parse_conjunction();
  std::unique_ptr<ast::Disjunction> parse_disjunction();
  ast::Condition parse_condition();
  std::unique_ptr<ast::Conjunction> parse_init_list();
  std::unique_ptr<ast::ActionDef> parse_action();
  std::unique_ptr<ast::ObjectsDef> parse_objects();
  std::unique_ptr<ast::InitDef> parse_init();
  std::unique_ptr<ast::GoalDef> parse_goal();
  std::unique_ptr<ast::FunctionsDef> parse_functions();
  std::unique_ptr<ast::MetricDef> parse_metric();
  std::unique_ptr<ast::Domain> parse_domain();
  std::unique_ptr<ast::Problem> parse_problem();
  void parse_domain(AST &ast);
  void parse_problem(AST &ast);

  Lexer lexer_;
};

} // namespace pddl

#endif /* end of include guard: PARSER_HPP */
