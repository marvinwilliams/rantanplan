#ifndef PARSER_HPP
#define PARSER_HPP

#include "parser/ast.hpp"
#include "parser/parser_exception.hpp"
#include "parser/rules.hpp"
#include "parser/tokens.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace parser {

template <typename TokenType, typename TokenIterator>
bool has_type(TokenIterator &token_iterator) {
  return (token_iterator.template has_type<TokenType>());
}

template <typename TokenIterator>
void skip_comments(TokenIterator &token_iterator) {
  while (has_type<tokens::Comment>(token_iterator)) {
    token_iterator++;
  }
}

template <typename TokenIterator> void advance(TokenIterator &token_iterator) {
  token_iterator++;
  skip_comments(token_iterator);
}

template <typename TokenType, typename TokenIterator>
void expect(TokenIterator &token_iterator) {
  if (!has_type<TokenType>(token_iterator)) {
    std::string msg = "Expected token \"" +
                      std::string(TokenType::printable_name) + "\" but got \"" +
                      token_iterator.to_string() + '\"';
    throw ParserException(token_iterator.location(), msg);
  }
}

template <typename TokenType, typename TokenIterator>
void skip(TokenIterator &token_iterator) {
  expect<TokenType>(token_iterator);
  token_iterator++;
}

template <typename TokenType, typename TokenIterator>
bool skip_if(TokenIterator &token_iterator) {
  if (has_type<TokenType>(token_iterator)) {
    token_iterator++;
    return true;
  }
  return false;
}

template <typename TokenType, typename TokenIterator>
TokenType get(TokenIterator &token_iterator) {
  expect<TokenType>(token_iterator);
  return std::get<TokenType>(*token_iterator);
}

template <typename TokenIterator>
std::unique_ptr<ast::NameList> parse_name_list(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  auto names = std::make_unique<std::vector<std::unique_ptr<ast::Name>>>();
  while (has_type<tokens::Identifier>(token_iterator)) {
    auto name = get<tokens::Identifier>(token_iterator).name;
    auto identifier =
        std::make_unique<ast::Name>(token_iterator.location(), name);
    names->push_back(std::move(identifier));
    token_iterator++;
  }
  return std::make_unique<ast::NameList>(begin + token_iterator.location(),
                                         std::move(names));
}

template <typename TokenIterator>
std::unique_ptr<ast::VariableList>
parse_variable_list(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  auto variables =
      std::make_unique<std::vector<std::unique_ptr<ast::Variable>>>();
  while (has_type<tokens::Variable>(token_iterator)) {
    auto name = get<tokens::Variable>(token_iterator).name;
    auto variable =
        std::make_unique<ast::Variable>(token_iterator.location(), name);
    variables->push_back(std::move(variable));
    token_iterator++;
  }
  return std::make_unique<ast::VariableList>(begin + token_iterator.location(),
                                             std::move(variables));
}

template <typename TokenIterator>
std::unique_ptr<ast::ArgumentList>
parse_argument_list(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  auto arguments =
      std::make_unique<std::vector<std::unique_ptr<ast::Argument>>>();
  while (has_type<tokens::Variable>(token_iterator) ||
         has_type<tokens::Identifier>(token_iterator)) {
    if (has_type<tokens::Variable>(token_iterator)) {
      auto name = get<tokens::Variable>(token_iterator).name;
      auto argument = std::make_unique<ast::Argument>(
          ast::Variable{token_iterator.location(), name});
      arguments->push_back(std::move(argument));
    } else {
      auto name = get<tokens::Identifier>(token_iterator).name;
      auto argument = std::make_unique<ast::Argument>(
          ast::Name{token_iterator.location(), name});
      arguments->push_back(std::move(argument));
    }
    token_iterator++;
  }
  return std::make_unique<ast::ArgumentList>(begin + token_iterator.location(),
                                             std::move(arguments));
}

template <typename TokenIterator>
std::unique_ptr<ast::TypedNameList>
parse_typed_name_list(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  auto lists = std::make_unique<
      std::vector<std::unique_ptr<ast::SingleTypedNameList>>>();
  while (has_type<tokens::Identifier>(token_iterator)) {
    auto inner_begin = token_iterator.location();
    auto name_list = parse_name_list(token_iterator);
    std::optional<std::unique_ptr<ast::Name>> type;
    if (skip_if<tokens::Hyphen>(token_iterator)) {
      type = std::make_unique<ast::Name>(
          token_iterator.location(),
          get<tokens::Identifier>(token_iterator).name);
      token_iterator++;
    }
    auto single_list = std::make_unique<ast::SingleTypedNameList>(
        inner_begin + token_iterator.location(), std::move(name_list),
        std::move(type));
    lists->push_back(std::move(single_list));
    skip_comments(token_iterator);
  }
  return std::make_unique<ast::TypedNameList>(begin + token_iterator.location(),
                                              std::move(lists));
}

template <typename TokenIterator>
std::unique_ptr<ast::TypedVariableList>
parse_typed_variable_list(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  auto lists = std::make_unique<
      std::vector<std::unique_ptr<ast::SingleTypedVariableList>>>();
  while (has_type<tokens::Variable>(token_iterator)) {
    auto inner_begin = token_iterator.location();
    auto variable_list = parse_variable_list(token_iterator);
    std::optional<std::unique_ptr<ast::Name>> type;
    if (skip_if<tokens::Hyphen>(token_iterator)) {
      type = std::make_unique<ast::Name>(
          token_iterator.location(),
          get<tokens::Identifier>(token_iterator).name);
      token_iterator++;
    }
    auto single_list = std::make_unique<ast::SingleTypedVariableList>(
        inner_begin + token_iterator.location(), std::move(variable_list),
        std::move(type));
    lists->push_back(std::move(single_list));
    skip_comments(token_iterator);
  }
  return std::make_unique<ast::TypedVariableList>(
      begin + token_iterator.location(), std::move(lists));
}

template <typename TokenIterator>
std::unique_ptr<ast::RequirementList>
parse_requirement_list(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  auto requirements =
      std::make_unique<std::vector<std::unique_ptr<ast::Requirement>>>();
  while (has_type<tokens::Section>(token_iterator)) {
    auto name = get<tokens::Section>(token_iterator).name;
    auto requirement =
        std::make_unique<ast::Requirement>(token_iterator.location(), name);
    requirements->push_back(std::move(requirement));
    token_iterator++;
  }
  return std::make_unique<ast::RequirementList>(
      begin + token_iterator.location(), std::move(requirements));
}

template <typename TokenIterator>
std::unique_ptr<ast::Element>
parse_requirements(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  advance(token_iterator);
  auto requirement_list = parse_requirement_list(token_iterator);
  return std::make_unique<ast::Element>(ast::RequirementsDef{
      begin + token_iterator.location(), std::move(requirement_list)});
}

template <typename TokenIterator>
std::unique_ptr<ast::Element> parse_types(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  advance(token_iterator);
  auto type_list = parse_typed_name_list(token_iterator);
  return std::make_unique<ast::Element>(
      ast::TypesDef{begin + token_iterator.location(), std::move(type_list)});
}

template <typename TokenIterator>
std::unique_ptr<ast::Element> parse_constants(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  advance(token_iterator);
  auto constant_list = parse_typed_name_list(token_iterator);
  return std::make_unique<ast::Element>(ast::ConstantsDef{
      begin + token_iterator.location(), std::move(constant_list)});
}

template <typename TokenIterator>
std::unique_ptr<ast::PredicateList>
parse_predicate_list(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  auto predicates =
      std::make_unique<std::vector<std::unique_ptr<ast::Predicate>>>();
  while (skip_if<tokens::LParen>(token_iterator)) {
    auto inner_begin = token_iterator.location();
    auto name = get<tokens::Identifier>(token_iterator).name;
    auto identifier =
        std::make_unique<ast::Name>(token_iterator.location(), name);
    advance(token_iterator);
    auto parameters = parse_typed_variable_list(token_iterator);
    auto predicate = std::make_unique<ast::Predicate>(
        inner_begin + token_iterator.location(), std::move(identifier),
        std::move(parameters));
    predicates->push_back(std::move(predicate));
    skip<tokens::RParen>(token_iterator);
    skip_comments(token_iterator);
  }
  return std::make_unique<ast::PredicateList>(begin + token_iterator.location(),
                                              std::move(predicates));
}

template <typename TokenIterator>
std::unique_ptr<ast::Element> parse_predicates(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  advance(token_iterator);
  auto predicate_list = parse_predicate_list(token_iterator);
  return std::make_unique<ast::Element>(ast::PredicatesDef{
      begin + token_iterator.location(), std::move(predicate_list)});
}

template <bool is_precondition, typename TokenIterator>
std::unique_ptr<ast::Condition> parse_condition(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  if (has_type<tokens::Identifier>(token_iterator)) {
    auto name = std::make_unique<ast::Name>(
        token_iterator.location(),
        get<tokens::Identifier>(token_iterator).name);
    advance(token_iterator);
    auto argument_list = parse_argument_list(token_iterator);
    return std::make_unique<ast::Condition>(
        ast::PredicateEvaluation{begin + token_iterator.location(),
                                 std::move(name), std::move(argument_list)});
  } else if (is_precondition && skip_if<tokens::Equality>(token_iterator)) {
    auto name = std::make_unique<ast::Name>(token_iterator.location(), "=");
    skip_comments(token_iterator);
    auto argument_list = parse_argument_list(token_iterator);
    return std::make_unique<ast::Condition>(
        ast::PredicateEvaluation{begin + token_iterator.location(),
                                 std::move(name), std::move(argument_list)});
  } else if (skip_if<tokens::And>(token_iterator)) {
    skip_comments(token_iterator);
    auto inner_begin = token_iterator.location();
    auto conditions =
        std::make_unique<std::vector<std::unique_ptr<ast::Condition>>>();
    while (skip_if<tokens::LParen>(token_iterator)) {
      skip_comments(token_iterator);
      auto condition = parse_condition<is_precondition>(token_iterator);
      conditions->push_back(std::move(condition));
      skip<tokens::RParen>(token_iterator);
      skip_comments(token_iterator);
    }
    auto condition_list = std::make_unique<ast::ConditionList>(
        inner_begin + token_iterator.location(), std::move(conditions));
    return std::make_unique<ast::Condition>(ast::Conjunction{
        begin + token_iterator.location(), std::move(condition_list)});
  } else if (is_precondition && skip_if<tokens::Or>(token_iterator)) {
    skip_comments(token_iterator);
    auto inner_begin = token_iterator.location();
    auto conditions =
        std::make_unique<std::vector<std::unique_ptr<ast::Condition>>>();
    while (skip_if<tokens::LParen>(token_iterator)) {
      skip_comments(token_iterator);
      auto condition = parse_condition<is_precondition>(token_iterator);
      conditions->push_back(std::move(condition));
      skip<tokens::RParen>(token_iterator);
      skip_comments(token_iterator);
    }
    auto condition_list = std::make_unique<ast::ConditionList>(
        inner_begin + token_iterator.location(), std::move(conditions));
    return std::make_unique<ast::Condition>(ast::Conjunction{
        begin + token_iterator.location(), std::move(condition_list)});
  } else if (skip_if<tokens::Not>(token_iterator)) {
    skip_comments(token_iterator);
    skip<tokens::LParen>(token_iterator);
    skip_comments(token_iterator);
    auto condition = parse_condition<is_precondition>(token_iterator);
    skip<tokens::RParen>(token_iterator);
    skip_comments(token_iterator);
    return std::make_unique<ast::Condition>(
        ast::Negation{begin + token_iterator.location(), std::move(condition)});
  }
  return std::make_unique<ast::Condition>();
}

template <typename TokenIterator>
std::unique_ptr<ast::Element> parse_action(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  token_iterator++;
  auto name = std::make_unique<ast::Name>(
      token_iterator.location(), get<tokens::Identifier>(token_iterator).name);
  advance(token_iterator);
  skip<tokens::Section>(token_iterator);
  skip_comments(token_iterator);
  skip<tokens::LParen>(token_iterator);
  skip_comments(token_iterator);
  auto parameters = parse_typed_variable_list(token_iterator);
  skip<tokens::RParen>(token_iterator);
  skip_comments(token_iterator);
  std::optional<std::unique_ptr<ast::Precondition>> precondition;
  if (has_type<tokens::Section>(token_iterator)) {
    auto section = get<tokens::Section>(token_iterator);
    if (section.name == "precondition") {
      auto inner_begin = token_iterator.location();
      advance(token_iterator);
      skip<tokens::LParen>(token_iterator);
      skip_comments(token_iterator);
      auto condition = parse_condition<true>(token_iterator);
      skip<tokens::RParen>(token_iterator);
      skip_comments(token_iterator);
      precondition = std::make_unique<ast::Precondition>(
          inner_begin + token_iterator.location(), std::move(condition));
    }
  }
  std::optional<std::unique_ptr<ast::Effect>> effect;
  if (has_type<tokens::Section>(token_iterator)) {
    auto section = get<tokens::Section>(token_iterator);
    if (section.name == "effect") {
      auto inner_begin = token_iterator.location();
      advance(token_iterator);
      skip<tokens::LParen>(token_iterator);
      skip_comments(token_iterator);
      auto condition = parse_condition<false>(token_iterator);
      skip<tokens::RParen>(token_iterator);
      skip_comments(token_iterator);
      effect = std::make_unique<ast::Effect>(
          inner_begin + token_iterator.location(), std::move(condition));
    }
  }
  return std::make_unique<ast::Element>(ast::ActionDef{
      begin + token_iterator.location(), std::move(name), std::move(parameters),
      std::move(precondition), std::move(effect)});
}

template <typename TokenIterator>
std::unique_ptr<ast::Element> parse_objects(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  advance(token_iterator);
  auto objects = parse_typed_name_list(token_iterator);
  return std::make_unique<ast::Element>(
      ast::ObjectsDef{begin + token_iterator.location(), std::move(objects)});
}

template <typename TokenIterator>
std::unique_ptr<ast::InitCondition>
parse_init_condition(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  auto name = std::make_unique<ast::Name>(
      token_iterator.location(), get<tokens::Identifier>(token_iterator).name);
  advance(token_iterator);
  auto argument_list = parse_name_list(token_iterator);
  return std::make_unique<ast::InitCondition>(
      ast::InitPredicate{begin + token_iterator.location(), std::move(name),
                         std::move(argument_list)});
}

template <typename TokenIterator>
std::unique_ptr<ast::InitList> parse_init_list(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  auto init_conditions =
      std::make_unique<std::vector<std::unique_ptr<ast::InitCondition>>>();
  while (skip_if<tokens::LParen>(token_iterator)) {
    skip_comments(token_iterator);
    if (has_type<tokens::Identifier>(token_iterator)) {
      auto init_condition = parse_init_condition(token_iterator);
      init_conditions->push_back(std::move(init_condition));
    } else if (has_type<tokens::Not>(token_iterator)) {
      auto inner_begin = token_iterator.location();
      advance(token_iterator);
      skip<tokens::LParen>(token_iterator);
      skip_comments(token_iterator);
      auto init_condition = parse_init_condition(token_iterator);
      skip<tokens::RParen>(token_iterator);
      skip_comments(token_iterator);
      auto init_negation = std::make_unique<ast::InitCondition>(
          ast::InitNegation{inner_begin + token_iterator.location(),
                            std::move(init_condition)});
      init_conditions->push_back(std::move(init_negation));
    }
    skip<tokens::RParen>(token_iterator);
  }
  return std::make_unique<ast::InitList>(begin + token_iterator.location(),
                                         std::move(init_conditions));
}

template <typename TokenIterator>
std::unique_ptr<ast::Element> parse_init(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  advance(token_iterator);
  auto init_list = parse_init_list(token_iterator);
  return std::make_unique<ast::Element>(
      ast::InitDef{begin + token_iterator.location(), std::move(init_list)});
}

template <typename TokenIterator>
std::unique_ptr<ast::Element> parse_goal(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  advance(token_iterator);
  skip<tokens::LParen>(token_iterator);
  skip_comments(token_iterator);
  auto goal_conditions = parse_condition<true>(token_iterator);
  skip<tokens::RParen>(token_iterator);
  return std::make_unique<ast::Element>(ast::GoalDef{
      begin + token_iterator.location(), std::move(goal_conditions)});
}

template <bool is_domain, typename TokenIterator>
std::unique_ptr<ast::Element> parse_element(TokenIterator &token_iterator) {
  auto section = get<tokens::Section>(token_iterator);
  if (section.name == "requirements") {
    return parse_requirements(token_iterator);
  } else if (is_domain && section.name == "types") {
    return parse_types(token_iterator);
  } else if (is_domain && section.name == "constants") {
    return parse_constants(token_iterator);
  } else if (is_domain && section.name == "predicates") {
    return parse_predicates(token_iterator);
  } else if (is_domain && section.name == "action") {
    return parse_action(token_iterator);
  } else if (!is_domain && section.name == "objects") {
    return parse_objects(token_iterator);
  } else if (!is_domain && section.name == "init") {
    return parse_init(token_iterator);
  } else if (!is_domain && section.name == "goal") {
    return parse_goal(token_iterator);
  } else {
    std::string msg = "Unknown section: \"" + section.name + "\"";
    throw ParserException(token_iterator.location(), msg);
  }
}

template <bool is_domain, typename TokenIterator>
std::unique_ptr<ast::ElementList>
parse_elements(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  auto elements =
      std::make_unique<std::vector<std::unique_ptr<ast::Element>>>();
  while (skip_if<tokens::LParen>(token_iterator)) {
    skip_comments(token_iterator);
    elements->push_back(parse_element<is_domain>(token_iterator));
    skip<tokens::RParen>(token_iterator);
    skip_comments(token_iterator);
  }
  return std::make_unique<ast::ElementList>(begin + token_iterator.location(),
                                            std::move(elements));
}

template <typename TokenIterator>
std::unique_ptr<ast::Domain> parse_domain(TokenIterator &token_iterator) {
  skip_comments(token_iterator);
  lexer::Location begin = token_iterator.location();
  skip<tokens::LParen>(token_iterator);
  skip<tokens::Define>(token_iterator);
  skip<tokens::LParen>(token_iterator);
  skip<tokens::Domain>(token_iterator);
  auto name = get<tokens::Identifier>(token_iterator).name;
  auto domain_name =
      std::make_unique<ast::Name>(token_iterator.location(), name);
  token_iterator++;
  skip<tokens::RParen>(token_iterator);
  skip_comments(token_iterator);
  auto domain_body = parse_elements<true>(token_iterator);
  skip<tokens::RParen>(token_iterator);
  return std::make_unique<ast::Domain>(begin + token_iterator.location(),
                                       std::move(domain_name),
                                       std::move(domain_body));
}

template <typename TokenIterator>
std::unique_ptr<ast::Problem> parse_problem(TokenIterator &token_iterator) {
  skip_comments(token_iterator);
  lexer::Location begin = token_iterator.location();
  skip<tokens::LParen>(token_iterator);
  skip<tokens::Define>(token_iterator);
  skip<tokens::LParen>(token_iterator);
  skip<tokens::Problem>(token_iterator);
  auto name = get<tokens::Identifier>(token_iterator).name;
  auto problem_name =
      std::make_unique<ast::Name>(token_iterator.location(), name);
  token_iterator++;
  skip<tokens::RParen>(token_iterator);
  skip<tokens::LParen>(token_iterator);
  skip<tokens::Section>(token_iterator);
  auto domain_ref = get<tokens::Identifier>(token_iterator).name;
  auto domain_ref_name =
      std::make_unique<ast::Name>(token_iterator.location(), domain_ref);
  token_iterator++;
  skip<tokens::RParen>(token_iterator);
  skip_comments(token_iterator);
  auto problem_body = parse_elements<false>(token_iterator);
  skip<tokens::RParen>(token_iterator);
  return std::make_unique<ast::Problem>(
      begin + token_iterator.location(), std::move(problem_name),
      std::move(domain_ref_name), std::move(problem_body));
}

template <typename TokenIterator>
void parse_domain(TokenIterator &token_iterator, ast::AST &ast) {
  auto domain = parse_domain(token_iterator);
  ast.set_domain(std::move(domain));
}

template <typename TokenIterator>
void parse_problem(TokenIterator &token_iterator, ast::AST &ast) {
  auto problem = parse_problem(token_iterator);
  ast.set_problem(std::move(problem));
}

} // namespace parser

#endif /* end of include guard: PARSER_HPP */
