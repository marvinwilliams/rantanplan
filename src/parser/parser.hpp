#ifndef PARSER_HPP
#define PARSER_HPP

#include "parser/ast.hpp"
#include "parser/parser_exception.hpp"
#include "parser/rules.hpp"
#include "parser/tokens.hpp"
#include "util/logger.hpp"
#include <config.hpp>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace parser {

static logging::Logger logger{"Parser"};

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
std::unique_ptr<ast::IdentifierList>
parse_identifier_list(TokenIterator &token_iterator) {
  const auto begin = token_iterator.location();
  auto names =
      std::make_unique<std::vector<std::unique_ptr<ast::Identifier>>>();
  while (has_type<tokens::Identifier>(token_iterator)) {
    const auto &name = get<tokens::Identifier>(token_iterator).name;
    LOG_DEBUG(logger, "Found identifier \"%s\"", name.c_str());
    auto identifier =
        std::make_unique<ast::Identifier>(token_iterator.location(), name);
    names->push_back(std::move(identifier));
    token_iterator++;
  }
  const auto &end = token_iterator.location();
  LOG_DEBUG(logger, "Number of elements: %u", names->size());
  return std::make_unique<ast::IdentifierList>(begin + end, std::move(names));
}

template <typename TokenIterator>
std::unique_ptr<ast::VariableList>
parse_variable_list(TokenIterator &token_iterator) {
  const auto begin = token_iterator.location();
  auto names = std::make_unique<std::vector<std::unique_ptr<ast::Variable>>>();
  while (has_type<tokens::Variable>(token_iterator)) {
    const auto &name = get<tokens::Variable>(token_iterator).name;
    LOG_DEBUG(logger, "Found variable \"?%s\"", name.c_str());
    auto variable =
        std::make_unique<ast::Variable>(token_iterator.location(), name);
    names->push_back(std::move(variable));
    token_iterator++;
  }
  const auto &end = token_iterator.location();
  LOG_DEBUG(logger, "Number of elements: %u", names->size());
  return std::make_unique<ast::VariableList>(begin + end, std::move(names));
}

template <typename TokenIterator>
std::unique_ptr<ast::ArgumentList>
parse_argument_list(TokenIterator &token_iterator) {
  const auto begin = token_iterator.location();
  auto arguments = std::make_unique<std::vector<ast::Argument>>();
  while (has_type<tokens::Identifier>(token_iterator) ||
         has_type<tokens::Variable>(token_iterator)) {
    if (has_type<tokens::Identifier>(token_iterator)) {
      const auto &name = get<tokens::Identifier>(token_iterator).name;
      LOG_DEBUG(logger, "Found argument(identifier) \"%s\"", name.c_str());
      auto argument =
          std::make_unique<ast::Identifier>(token_iterator.location(), name);
      arguments->push_back(std::move(argument));
    } else {
      const auto &name = get<tokens::Variable>(token_iterator).name;
      LOG_DEBUG(logger, "Found argument(variable) \"?%s\"", name.c_str());
      auto argument =
          std::make_unique<ast::Variable>(token_iterator.location(), name);
      arguments->push_back(std::move(argument));
    }
    token_iterator++;
  }
  const auto &end = token_iterator.location();
  LOG_DEBUG(logger, "Number of elements: %u", arguments->size());
  return std::make_unique<ast::ArgumentList>(begin + end, std::move(arguments));
}

template <typename TokenIterator>
std::unique_ptr<ast::SingleTypeIdentifierList>
parse_single_type_identifier_list(TokenIterator &token_iterator) {
  const auto begin = token_iterator.location();
  auto name_list = parse_identifier_list(token_iterator);
  if (skip_if<tokens::Hyphen>(token_iterator)) {
    const auto &name = get<tokens::Identifier>(token_iterator).name;
    LOG_DEBUG(logger, "Found type \"%s\"", name.c_str());
    auto type =
        std::make_unique<ast::Identifier>(token_iterator.location(), name);
    token_iterator++;
    const auto &end = type->location;
    return std::make_unique<ast::SingleTypeIdentifierList>(
        begin + end, std::move(name_list), std::move(type));
  }
  return std::make_unique<ast::SingleTypeIdentifierList>(name_list->location,
                                                         std::move(name_list));
}

template <typename TokenIterator>
std::unique_ptr<ast::SingleTypeVariableList>
parse_single_type_variable_list(TokenIterator &token_iterator) {
  const auto begin = token_iterator.location();
  auto variable_list = parse_variable_list(token_iterator);
  if (skip_if<tokens::Hyphen>(token_iterator)) {
    const auto &name = get<tokens::Identifier>(token_iterator).name;
    LOG_DEBUG(logger, "Found type \"%s\"", name.c_str());
    auto type =
        std::make_unique<ast::Identifier>(token_iterator.location(), name);
    token_iterator++;
    const auto &end = type->location;
    return std::make_unique<ast::SingleTypeVariableList>(
        begin + end, std::move(variable_list), std::move(type));
  }
  return std::make_unique<ast::SingleTypeVariableList>(
      variable_list->location, std::move(variable_list));
}

template <typename TokenIterator>
std::unique_ptr<ast::TypedIdentifierList>
parse_typed_identifier_list(TokenIterator &token_iterator) {
  LOG_DEBUG(logger, "Parsing typed identifier list");
  const auto begin = token_iterator.location();
  auto lists = std::make_unique<
      std::vector<std::unique_ptr<ast::SingleTypeIdentifierList>>>();
  while (has_type<tokens::Identifier>(token_iterator)) {
    auto single_list = parse_single_type_identifier_list(token_iterator);
    lists->push_back(std::move(single_list));
    skip_comments(token_iterator);
  }
  const auto &end = token_iterator.location();
  return std::make_unique<ast::TypedIdentifierList>(begin + end,
                                                    std::move(lists));
}

template <typename TokenIterator>
std::unique_ptr<ast::TypedVariableList>
parse_typed_variable_list(TokenIterator &token_iterator) {
  LOG_DEBUG(logger, "Parsing typed variable list");
  const auto begin = token_iterator.location();
  auto lists = std::make_unique<
      std::vector<std::unique_ptr<ast::SingleTypeVariableList>>>();
  while (has_type<tokens::Variable>(token_iterator)) {
    auto single_list = parse_single_type_variable_list(token_iterator);
    lists->push_back(std::move(single_list));
    skip_comments(token_iterator);
  }
  const auto &end = token_iterator.location();
  return std::make_unique<ast::TypedVariableList>(begin + end,
                                                  std::move(lists));
}

template <typename TokenIterator>
std::unique_ptr<ast::RequirementList>
parse_requirement_list(TokenIterator &token_iterator) {
  const auto begin = token_iterator.location();
  auto requirements =
      std::make_unique<std::vector<std::unique_ptr<ast::Requirement>>>();
  while (has_type<tokens::Section>(token_iterator)) {
    const auto &name = get<tokens::Section>(token_iterator).name;
    auto requirement =
        std::make_unique<ast::Requirement>(token_iterator.location(), name);
    requirements->push_back(std::move(requirement));
    token_iterator++;
  }
  const auto &end = token_iterator.location();
  return std::make_unique<ast::RequirementList>(begin + end,
                                                std::move(requirements));
}

template <typename TokenIterator>
std::unique_ptr<ast::RequirementsDef>
parse_requirements(TokenIterator &token_iterator) {
  LOG_DEBUG(logger, "Parsing requirements");
  const auto begin = token_iterator.location();
  advance(token_iterator);
  auto requirement_list = parse_requirement_list(token_iterator);
  const auto &end = requirement_list->location;
  LOG_DEBUG(logger, "Number of requirements: %u",
            requirement_list->elements->size());
  return std::make_unique<ast::RequirementsDef>(begin + end,
                                                std::move(requirement_list));
}

template <typename TokenIterator>
std::unique_ptr<ast::TypesDef> parse_types(TokenIterator &token_iterator) {
  LOG_DEBUG(logger, "Parsing types");
  const auto begin = token_iterator.location();
  advance(token_iterator);
  auto type_list = parse_typed_identifier_list(token_iterator);
  const auto &end = type_list->location;
  return std::make_unique<ast::TypesDef>(begin + end, std::move(type_list));
}

template <typename TokenIterator>
std::unique_ptr<ast::ConstantsDef>
parse_constants(TokenIterator &token_iterator) {
  LOG_DEBUG(logger, "Parsing constants");
  auto begin = token_iterator.location();
  advance(token_iterator);
  auto constant_list = parse_typed_identifier_list(token_iterator);
  const auto &end = constant_list->location;
  return std::make_unique<ast::ConstantsDef>(begin + end,
                                             std::move(constant_list));
}

template <typename TokenIterator>
std::unique_ptr<ast::Predicate> parse_predicate(TokenIterator &token_iterator) {
  const auto begin = token_iterator.location();
  const auto &name = get<tokens::Identifier>(token_iterator).name;
  LOG_DEBUG(logger, "Found predicate \"%s\"", name.c_str());
  auto identifier =
      std::make_unique<ast::Identifier>(token_iterator.location(), name);
  advance(token_iterator);
  auto parameters = parse_typed_variable_list(token_iterator);
  const auto &end = parameters->location;
  return std::make_unique<ast::Predicate>(begin + end, std::move(identifier),
                                          std::move(parameters));
}

template <typename TokenIterator>
std::unique_ptr<ast::PredicateList>
parse_predicate_list(TokenIterator &token_iterator) {
  auto begin = token_iterator.location();
  auto predicates =
      std::make_unique<std::vector<std::unique_ptr<ast::Predicate>>>();
  while (skip_if<tokens::LParen>(token_iterator)) {
    auto predicate = parse_predicate(token_iterator);
    predicates->push_back(std::move(predicate));
    skip<tokens::RParen>(token_iterator);
    skip_comments(token_iterator);
  }
  const auto &end = token_iterator.location();
  return std::make_unique<ast::PredicateList>(begin + end,
                                              std::move(predicates));
}

template <typename TokenIterator>
std::unique_ptr<ast::PredicatesDef>
parse_predicates(TokenIterator &token_iterator) {
  LOG_DEBUG(logger, "Parsing predicates");
  const auto begin = token_iterator.location();
  advance(token_iterator);
  auto predicate_list = parse_predicate_list(token_iterator);
  const auto &end = predicate_list->location;
  LOG_DEBUG(logger, "Number of predicates: %u", predicate_list->elements->size());
  return std::make_unique<ast::PredicatesDef>(begin + end,
                                              std::move(predicate_list));
}

template <typename TokenIterator>
ast::Condition parse_condition(TokenIterator &);

template <typename TokenIterator>
std::unique_ptr<ast::PredicateEvaluation>
parse_predicate_evaluation(TokenIterator &token_iterator) {
  LOG_DEBUG(logger, "Parsing predicate evaluation");
  const auto begin = token_iterator.location();
  const std::string &name = (has_type<tokens::Equality>(token_iterator))
                                ? "="
                                : get<tokens::Identifier>(token_iterator).name;
  LOG_DEBUG(logger, "Found call to predicate \"%s\"", name.c_str());
  auto predicate_name =
      std::make_unique<ast::Identifier>(token_iterator.location(), name);
  advance(token_iterator);
  auto argument_list = parse_argument_list(token_iterator);
  const auto &end = argument_list->location;
  return std::make_unique<ast::PredicateEvaluation>(
      begin + end, std::move(predicate_name), std::move(argument_list));
}

template <typename TokenIterator>
std::unique_ptr<ast::Conjunction>
parse_conjunction(TokenIterator &token_iterator) {
  LOG_DEBUG(logger, "Parsing conjunction");
  const auto begin = token_iterator.location();
  advance(token_iterator);
  auto arguments = std::make_unique<std::vector<ast::Condition>>();
  while (skip_if<tokens::LParen>(token_iterator)) {
    skip_comments(token_iterator);
    auto argument = parse_condition(token_iterator);
    arguments->push_back(std::move(argument));
    skip<tokens::RParen>(token_iterator);
    skip_comments(token_iterator);
  }
  const auto &end = token_iterator.location();
  auto condition_list =
      std::make_unique<ast::ConditionList>(begin + end, std::move(arguments));
  return std::make_unique<ast::Conjunction>(condition_list->location,
                                            std::move(condition_list));
}

template <typename TokenIterator>
std::unique_ptr<ast::Disjunction>
parse_disjunction(TokenIterator &token_iterator) {
  LOG_DEBUG(logger, "Parsing disjunction");
  const auto begin = token_iterator.location();
  advance(token_iterator);
  auto arguments = std::make_unique<std::vector<ast::Condition>>();
  while (skip_if<tokens::LParen>(token_iterator)) {
    skip_comments(token_iterator);
    auto argument = parse_condition(token_iterator);
    arguments->push_back(std::move(argument));
    skip<tokens::RParen>(token_iterator);
    skip_comments(token_iterator);
  }
  const auto &end = token_iterator.location();
  auto condition_list =
      std::make_unique<ast::ConditionList>(begin + end, std::move(arguments));
  return std::make_unique<ast::Disjunction>(condition_list->location,
                                            std::move(condition_list));
}

template <typename TokenIterator>
ast::Condition parse_condition(TokenIterator &token_iterator) {
  if (has_type<tokens::Identifier>(token_iterator) ||
      has_type<tokens::Equality>(token_iterator)) {
    return parse_predicate_evaluation(token_iterator);
  } else if (has_type<tokens::And>(token_iterator)) {
    return parse_conjunction(token_iterator);
  } else if (has_type<tokens::Or>(token_iterator)) {
    return parse_disjunction(token_iterator);
  } else if (has_type<tokens::Not>(token_iterator)) {
    LOG_DEBUG(logger, "Parsing negation");
    const auto begin = token_iterator.location();
    advance(token_iterator);
    skip<tokens::LParen>(token_iterator);
    skip_comments(token_iterator);
    auto condition = parse_condition(token_iterator);
    skip<tokens::RParen>(token_iterator);
    skip_comments(token_iterator);
    const auto &end = token_iterator.location();
    return std::make_unique<ast::Negation>(begin + end, std::move(condition));
  }
  return std::make_unique<ast::Empty>();
}

template <typename TokenIterator>
std::unique_ptr<ast::Conjunction>
parse_init_list(TokenIterator &token_iterator) {
  const auto begin = token_iterator.location();
  auto arguments = std::make_unique<std::vector<ast::Condition>>();
  while (skip_if<tokens::LParen>(token_iterator)) {
    skip_comments(token_iterator);
    if (has_type<tokens::Not>(token_iterator)) {
      LOG_DEBUG(logger, "Parsing negation");
      const auto begin_negation = token_iterator.location();
      advance(token_iterator);
      skip<tokens::LParen>(token_iterator);
      skip_comments(token_iterator);
      auto argument = parse_predicate_evaluation(token_iterator);
      skip<tokens::RParen>(token_iterator);
      skip_comments(token_iterator);
      const auto &end = token_iterator.location();
      auto negation = std::make_unique<ast::Negation>(begin_negation + end,
                                                      std::move(argument));
      arguments->push_back(std::move(negation));
    } else {
      auto argument = parse_predicate_evaluation(token_iterator);
      arguments->push_back(std::move(argument));
    }
    skip<tokens::RParen>(token_iterator);
    skip_comments(token_iterator);
  }
  LOG_DEBUG(logger, "Number of init predicates: %u", arguments->size());
  const auto &end = token_iterator.location();
  auto condition_list =
      std::make_unique<ast::ConditionList>(begin + end, std::move(arguments));
  return std::make_unique<ast::Conjunction>(condition_list->location,
                                            std::move(condition_list));
}

template <typename TokenIterator>
std::unique_ptr<ast::ActionDef> parse_action(TokenIterator &token_iterator) {
  LOG_DEBUG(logger, "Parsing action");
  const auto begin = token_iterator.location();
  token_iterator++;
  const auto &name = get<tokens::Identifier>(token_iterator).name;
  LOG_DEBUG(logger, "Found action \"%s\"", name.c_str());
  auto action_name = std::make_unique<ast::Identifier>(
      token_iterator.location(), std::move(name));
  advance(token_iterator);
  skip<tokens::Section>(token_iterator);
  skip_comments(token_iterator);
  skip<tokens::LParen>(token_iterator);
  skip_comments(token_iterator);
  auto parameters = parse_typed_variable_list(token_iterator);
  skip<tokens::RParen>(token_iterator);
  skip_comments(token_iterator);
  std::unique_ptr<ast::Precondition> precondition = nullptr;
  if (has_type<tokens::Section>(token_iterator)) {
    const auto &section = get<tokens::Section>(token_iterator);
    if (section.name == "precondition") {
      LOG_DEBUG(logger, "Parsing precondition");
      const auto precondition_begin = token_iterator.location();
      advance(token_iterator);
      skip<tokens::LParen>(token_iterator);
      skip_comments(token_iterator);
      auto condition = parse_condition(token_iterator);
      skip<tokens::RParen>(token_iterator);
      const auto end = token_iterator.location();
      skip_comments(token_iterator);
      precondition = std::make_unique<ast::Precondition>(
          precondition_begin + end, std::move(condition));
    }
  }
  std::unique_ptr<ast::Effect> effect = nullptr;
  if (has_type<tokens::Section>(token_iterator)) {
    const auto &section = get<tokens::Section>(token_iterator);
    if (section.name == "effect") {
      LOG_DEBUG(logger, "Parsing effect");
      const auto effect_begin = token_iterator.location();
      advance(token_iterator);
      skip<tokens::LParen>(token_iterator);
      skip_comments(token_iterator);
      auto condition = parse_condition(token_iterator);
      skip<tokens::RParen>(token_iterator);
      const auto end = token_iterator.location();
      skip_comments(token_iterator);
      effect = std::make_unique<ast::Effect>(effect_begin + end,
                                             std::move(condition));
    }
  }
  const auto &end = token_iterator.location();
  return std::make_unique<ast::ActionDef>(
      begin + end, std::move(action_name), std::move(parameters),
      std::move(precondition), std::move(effect));
}

template <typename TokenIterator>
std::unique_ptr<ast::ObjectsDef> parse_objects(TokenIterator &token_iterator) {
  LOG_DEBUG(logger, "Parsing objects");
  const auto begin = token_iterator.location();
  advance(token_iterator);
  auto objects = parse_typed_identifier_list(token_iterator);
  const auto &end = objects->location;
  return std::make_unique<ast::ObjectsDef>(begin + end, std::move(objects));
}

template <typename TokenIterator>
std::unique_ptr<ast::InitDef> parse_init(TokenIterator &token_iterator) {
  LOG_DEBUG(logger, "Parsing init list");
  const auto begin = token_iterator.location();
  advance(token_iterator);
  auto init_list = parse_init_list(token_iterator);
  const auto &end = token_iterator.location();
  return std::make_unique<ast::InitDef>(begin + end, std::move(init_list));
}

template <typename TokenIterator>
std::unique_ptr<ast::GoalDef> parse_goal(TokenIterator &token_iterator) {
  LOG_DEBUG(logger, "Parsing goal");
  const auto begin = token_iterator.location();
  advance(token_iterator);
  skip<tokens::LParen>(token_iterator);
  skip_comments(token_iterator);
  auto condition = parse_condition(token_iterator);
  skip<tokens::RParen>(token_iterator);
  const auto &end = token_iterator.location();
  return std::make_unique<ast::GoalDef>(begin + end, std::move(condition));
}

template <bool is_domain, typename TokenIterator>
ast::Element parse_element(TokenIterator &token_iterator) {
  const auto &section = get<tokens::Section>(token_iterator).name;
  if (section == "requirements") {
    return parse_requirements(token_iterator);
  } else if (is_domain && section == "types") {
    return parse_types(token_iterator);
  } else if (is_domain && section == "constants") {
    return parse_constants(token_iterator);
  } else if (is_domain && section == "predicates") {
    return parse_predicates(token_iterator);
  } else if (is_domain && section == "action") {
    return parse_action(token_iterator);
  } else if (!is_domain && section == "objects") {
    return parse_objects(token_iterator);
  } else if (!is_domain && section == "init") {
    return parse_init(token_iterator);
  } else if (!is_domain && section == "goal") {
    return parse_goal(token_iterator);
  } else {
    LOG_ERROR(logger, "No match for section \"%s\"", section.c_str());
    std::string msg = "Unknown section: \"" + section + "\"";
    throw ParserException(token_iterator.location(), msg);
  }
}

template <bool is_domain, typename TokenIterator>
std::unique_ptr<ast::ElementList>
parse_elements(TokenIterator &token_iterator) {
  const auto begin = token_iterator.location();
  auto elements = std::make_unique<std::vector<ast::Element>>();
  while (skip_if<tokens::LParen>(token_iterator)) {
    skip_comments(token_iterator);
    elements->push_back(parse_element<is_domain>(token_iterator));
    skip<tokens::RParen>(token_iterator);
    skip_comments(token_iterator);
  }
  const auto &end = token_iterator.location();
  return std::make_unique<ast::ElementList>(begin + end, std::move(elements));
}

template <typename TokenIterator>
std::unique_ptr<ast::Domain> parse_domain(TokenIterator &token_iterator) {
  LOG_DEBUG(logger, "Parsing domain");
  const auto begin = token_iterator.location();
  skip<tokens::LParen>(token_iterator);
  skip<tokens::Define>(token_iterator);
  skip<tokens::LParen>(token_iterator);
  skip<tokens::Domain>(token_iterator);
  const auto &name = get<tokens::Identifier>(token_iterator).name;
  LOG_DEBUG(logger, "Found name \"%s\"", name.c_str());
  auto domain_name =
      std::make_unique<ast::Identifier>(token_iterator.location(), name);
  token_iterator++;
  skip<tokens::RParen>(token_iterator);
  skip_comments(token_iterator);
  auto domain_body = parse_elements<true>(token_iterator);
  skip<tokens::RParen>(token_iterator);
  const auto &end = token_iterator.location();
  return std::make_unique<ast::Domain>(begin + end, std::move(domain_name),
                                       std::move(domain_body));
}

template <typename TokenIterator>
std::unique_ptr<ast::Problem> parse_problem(TokenIterator &token_iterator) {
  LOG_DEBUG(logger, "Parsing problem");
  const auto begin = token_iterator.location();
  skip<tokens::LParen>(token_iterator);
  skip<tokens::Define>(token_iterator);
  skip<tokens::LParen>(token_iterator);
  skip<tokens::Problem>(token_iterator);
  const auto &name = get<tokens::Identifier>(token_iterator).name;
  LOG_DEBUG(logger, "Found name \"%s\"", name.c_str());
  auto problem_name =
      std::make_unique<ast::Identifier>(token_iterator.location(), name);
  token_iterator++;
  skip<tokens::RParen>(token_iterator);
  skip<tokens::LParen>(token_iterator);
  skip<tokens::Section>(token_iterator);
  const auto &domain_ref = get<tokens::Identifier>(token_iterator).name;
  LOG_DEBUG(logger, "Found reference to domain \"%s\"", name.c_str());
  auto domain_ref_name =
      std::make_unique<ast::Identifier>(token_iterator.location(), domain_ref);
  token_iterator++;
  skip<tokens::RParen>(token_iterator);
  skip_comments(token_iterator);
  auto problem_body = parse_elements<false>(token_iterator);
  skip<tokens::RParen>(token_iterator);
  const auto &end = token_iterator.location();
  return std::make_unique<ast::Problem>(begin + end, std::move(problem_name),
                                        std::move(domain_ref_name),
                                        std::move(problem_body));
}

template <typename TokenIterator>
void parse_domain(TokenIterator &token_iterator, ast::AST &ast) {
  skip_comments(token_iterator);
  auto domain = parse_domain(token_iterator);
  ast.set_domain(std::move(domain));
}

template <typename TokenIterator>
void parse_problem(TokenIterator &token_iterator, ast::AST &ast) {
  skip_comments(token_iterator);
  auto problem = parse_problem(token_iterator);
  ast.set_problem(std::move(problem));
}

} // namespace parser

#endif /* end of include guard: PARSER_HPP */
