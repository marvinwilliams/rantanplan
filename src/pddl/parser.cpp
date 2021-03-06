#include "pddl/parser.hpp"
#include "logging/logging.hpp"
#include "pddl/ast/ast.hpp"
#include "pddl/parser_exception.hpp"
#include "pddl/tokens.hpp"

#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace pddl {

ast::AST Parser::parse(const std::string &domain, const std::string &problem) {
  ast::AST ast;
  std::ifstream domain_in{domain};
  std::string domain_bytes{std::istreambuf_iterator<char>{domain_in},
                           std::istreambuf_iterator<char>{}};
  std::ifstream problem_in{problem};
  std::string problem_bytes{std::istreambuf_iterator<char>{problem_in},
                            std::istreambuf_iterator<char>{}};
  if (!domain_in.is_open()) {
    throw ParserException{"Failed to open " + domain};
  }
  if (!problem_in.is_open()) {
    throw ParserException{"Failed to open " + problem};
  }
  lexer_.set_source(domain, domain_bytes.data(),
                    domain_bytes.data() + domain_bytes.size());
  LOG_INFO(parser_logger, "Parsing domain file...");
  parse_domain(ast);
  lexer_.set_source(problem, problem_bytes.data(),
                    problem_bytes.data() + problem_bytes.size());
  LOG_INFO(parser_logger, "Parsing problem file...");
  parse_problem(ast);
  return ast;
}

void Parser::skip_comments() {
  while (lexer_.has_type<token::Comment>()) {
    lexer_.next();
  }
}

void Parser::advance() {
  lexer_.next();
  skip_comments();
}

std::unique_ptr<ast::IdentifierList> Parser::parse_identifier_list() {
  LOG_DEBUG(parser_logger, "Parsing identifier list");
  const auto begin = lexer_.location();
  auto names =
      std::make_unique<std::vector<std::unique_ptr<ast::Identifier>>>();
  while (lexer_.has_type<token::Name>()) {
    const auto &name = lexer_.get<token::Name>().name;
    LOG_DEBUG(parser_logger, "Found identifier \'%s\'", std::string{name}.c_str());
    auto identifier =
        std::make_unique<ast::Identifier>(lexer_.location(), name);
    names->push_back(std::move(identifier));
    lexer_.next();
  }
  const auto &end = lexer_.location();
  LOG_DEBUG(parser_logger, "End of identifier list (%u element(s))", names->size());
  return std::make_unique<ast::IdentifierList>(begin + end, std::move(names));
}

std::unique_ptr<ast::VariableList> Parser::parse_variable_list() {
  LOG_DEBUG(parser_logger, "Parsing variable list");
  const auto begin = lexer_.location();
  auto names = std::make_unique<std::vector<std::unique_ptr<ast::Variable>>>();
  while (lexer_.has_type<token::Variable>()) {
    const auto &name = lexer_.get<token::Variable>().name;
    LOG_DEBUG(parser_logger, "Found variable \'%s\'", std::string{name}.c_str());
    auto variable = std::make_unique<ast::Variable>(lexer_.location(), name);
    names->push_back(std::move(variable));
    lexer_.next();
  }
  const auto &end = lexer_.location();
  LOG_DEBUG(parser_logger, "End of variable list (%u element(s))", names->size());
  return std::make_unique<ast::VariableList>(begin + end, std::move(names));
}

std::unique_ptr<ast::ArgumentList> Parser::parse_argument_list() {
  LOG_DEBUG(parser_logger, "Parsing argument list");
  const auto begin = lexer_.location();
  auto arguments = std::make_unique<std::vector<ast::Argument>>();
  while (lexer_.has_type<token::Name>() || lexer_.has_type<token::Variable>()) {
    if (lexer_.has_type<token::Name>()) {
      const auto &name = lexer_.get<token::Name>().name;
      LOG_DEBUG(parser_logger, "Found identifier \'%s\'", std::string{name}.c_str());
      auto argument =
          std::make_unique<ast::Identifier>(lexer_.location(), name);
      arguments->push_back(std::move(argument));
    } else {
      const auto &name = lexer_.get<token::Variable>().name;
      LOG_DEBUG(parser_logger, "Found variable \'%s\'", std::string{name}.c_str());
      auto argument = std::make_unique<ast::Variable>(lexer_.location(), name);
      arguments->push_back(std::move(argument));
    }
    lexer_.next();
  }
  const auto &end = lexer_.location();
  LOG_DEBUG(parser_logger, "End of argument list (%u element(s))", arguments->size());
  return std::make_unique<ast::ArgumentList>(begin + end, std::move(arguments));
}

std::unique_ptr<ast::SingleTypeIdentifierList>
Parser::parse_single_type_identifier_list() {
  LOG_DEBUG(parser_logger, "Parsing single type identifier list");
  const auto begin = lexer_.location();
  auto name_list = parse_identifier_list();
  if (skip_if<token::Hyphen>()) {
    const auto &name = lexer_.get<token::Name>().name;
    LOG_DEBUG(parser_logger, "Found type \'%s\'", std::string{name}.c_str());
    auto type = std::make_unique<ast::Identifier>(lexer_.location(), name);
    lexer_.next();
    const auto &end = type->location;
    LOG_DEBUG(parser_logger, "End of single type identifier list");
    return std::make_unique<ast::SingleTypeIdentifierList>(
        begin + end, std::move(name_list), std::move(type));
  } else {
    LOG_DEBUG(parser_logger, "End of single type identifier list");
    return std::make_unique<ast::SingleTypeIdentifierList>(
        name_list->location, std::move(name_list));
  }
}

std::unique_ptr<ast::SingleTypeVariableList>
Parser::parse_single_type_variable_list() {
  LOG_DEBUG(parser_logger, "Parsing single type variable list");
  const auto begin = lexer_.location();
  auto variable_list = parse_variable_list();
  if (skip_if<token::Hyphen>()) {
    const auto &name = lexer_.get<token::Name>().name;
    LOG_DEBUG(parser_logger, "Found type \'%s\'", std::string{name}.c_str());
    auto type = std::make_unique<ast::Identifier>(lexer_.location(), name);
    lexer_.next();
    const auto &end = type->location;
    LOG_DEBUG(parser_logger, "End of single type variable list");
    return std::make_unique<ast::SingleTypeVariableList>(
        begin + end, std::move(variable_list), std::move(type));
  } else {
    LOG_DEBUG(parser_logger, "End of single type variable list");
    return std::make_unique<ast::SingleTypeVariableList>(
        variable_list->location, std::move(variable_list));
  }
}

std::unique_ptr<ast::TypedIdentifierList>
Parser::parse_typed_identifier_list() {
  LOG_DEBUG(parser_logger, "Parsing typed identifier list");
  const auto begin = lexer_.location();
  auto lists = std::make_unique<
      std::vector<std::unique_ptr<ast::SingleTypeIdentifierList>>>();
  while (lexer_.has_type<token::Name>()) {
    auto single_list = parse_single_type_identifier_list();
    lists->push_back(std::move(single_list));
    skip_comments();
  }
  const auto &end = lexer_.location();
  LOG_DEBUG(parser_logger, "End of typed identifier list (%u single type lists)",
            lists->size());
  return std::make_unique<ast::TypedIdentifierList>(begin + end,
                                                    std::move(lists));
}

std::unique_ptr<ast::TypedVariableList> Parser::parse_typed_variable_list() {
  LOG_DEBUG(parser_logger, "Parsing typed variable list");
  const auto begin = lexer_.location();
  auto lists = std::make_unique<
      std::vector<std::unique_ptr<ast::SingleTypeVariableList>>>();
  while (lexer_.has_type<token::Variable>()) {
    auto single_list = parse_single_type_variable_list();
    lists->push_back(std::move(single_list));
    skip_comments();
  }
  const auto &end = lexer_.location();
  LOG_DEBUG(parser_logger, "End of typed variable list (%u single type lists)",
            lists->size());
  return std::make_unique<ast::TypedVariableList>(begin + end,
                                                  std::move(lists));
}

std::unique_ptr<ast::RequirementList> Parser::parse_requirement_list() {
  LOG_DEBUG(parser_logger, "Parsing requirements list");
  const auto begin = lexer_.location();
  auto requirements =
      std::make_unique<std::vector<std::unique_ptr<ast::Requirement>>>();
  while (lexer_.has_type<token::Requirement>()) {
    const auto &name = lexer_.get<token::Requirement>().name;
    LOG_DEBUG(parser_logger, "Found requirement \'%s\'", std::string{name}.c_str());
    auto requirement =
        std::make_unique<ast::Requirement>(lexer_.location(), name);
    requirements->push_back(std::move(requirement));
    lexer_.next();
  }
  const auto &end = lexer_.location();
  LOG_DEBUG(parser_logger, "End of requirements list (%u element(s))",
            requirements->size());
  return std::make_unique<ast::RequirementList>(begin + end,
                                                std::move(requirements));
}

std::unique_ptr<ast::RequirementsDef> Parser::parse_requirements() {
  LOG_DEBUG(parser_logger, "Parsing requirements definition");
  const auto begin = lexer_.location();
  advance();
  auto requirement_list = parse_requirement_list();
  const auto &end = requirement_list->location;
  LOG_DEBUG(parser_logger, "End of requirements definition");
  return std::make_unique<ast::RequirementsDef>(begin + end,
                                                std::move(requirement_list));
}

std::unique_ptr<ast::TypesDef> Parser::parse_types() {
  LOG_DEBUG(parser_logger, "Parsing types definition");
  const auto begin = lexer_.location();
  advance();
  auto type_list = parse_typed_identifier_list();
  const auto &end = type_list->location;
  LOG_DEBUG(parser_logger, "End of types definition");
  return std::make_unique<ast::TypesDef>(begin + end, std::move(type_list));
}

std::unique_ptr<ast::ConstantsDef> Parser::parse_constants() {
  LOG_DEBUG(parser_logger, "Parsing constants definition");
  auto begin = lexer_.location();
  advance();
  auto constant_list = parse_typed_identifier_list();
  const auto &end = constant_list->location;
  LOG_DEBUG(parser_logger, "End of constants definition");
  return std::make_unique<ast::ConstantsDef>(begin + end,
                                             std::move(constant_list));
}

std::unique_ptr<ast::Predicate> Parser::parse_predicate() {
  LOG_DEBUG(parser_logger, "Parsing predicate");
  const auto begin = lexer_.location();
  const auto &name = lexer_.get<token::Name>().name;
  LOG_DEBUG(parser_logger, "Found predicate name \'%s\'", std::string{name}.c_str());
  auto identifier = std::make_unique<ast::Identifier>(lexer_.location(), name);
  advance();
  auto parameters = parse_typed_variable_list();
  const auto &end = parameters->location;
  LOG_DEBUG(parser_logger, "End of predicate");
  return std::make_unique<ast::Predicate>(begin + end, std::move(identifier),
                                          std::move(parameters));
}

std::unique_ptr<ast::PredicateList> Parser::parse_predicate_list() {
  LOG_DEBUG(parser_logger, "Parsing predicate list");
  auto begin = lexer_.location();
  auto predicates =
      std::make_unique<std::vector<std::unique_ptr<ast::Predicate>>>();
  while (skip_if<token::LParen>()) {
    auto predicate = parse_predicate();
    predicates->push_back(std::move(predicate));
    skip<token::RParen>();
    skip_comments();
  }
  const auto &end = lexer_.location();
  LOG_DEBUG(parser_logger, "End of predicate list (%u element(s))",
            predicates->size());
  return std::make_unique<ast::PredicateList>(begin + end,
                                              std::move(predicates));
}

std::unique_ptr<ast::PredicatesDef> Parser::parse_predicates() {
  LOG_DEBUG(parser_logger, "Parsing predicates definition");
  const auto begin = lexer_.location();
  advance();
  auto predicate_list = parse_predicate_list();
  const auto &end = predicate_list->location;
  LOG_DEBUG(parser_logger, "End of predicates definition");
  return std::make_unique<ast::PredicatesDef>(begin + end,
                                              std::move(predicate_list));
}

std::unique_ptr<ast::PredicateEvaluation> Parser::parse_predicate_evaluation() {
  LOG_DEBUG(parser_logger, "Parsing predicate evaluation");
  const auto begin = lexer_.location();
  const auto &name = (lexer_.has_type<token::Equality>())
                         ? "="
                         : lexer_.get<token::Name>().name;
  LOG_DEBUG(parser_logger, "Found predicate \'%s\'", std::string{name}.c_str());
  auto predicate_name =
      std::make_unique<ast::Identifier>(lexer_.location(), name);
  advance();
  auto argument_list = parse_argument_list();
  const auto &end = argument_list->location;
  LOG_DEBUG(parser_logger, "End of predicate evaluation");
  return std::make_unique<ast::PredicateEvaluation>(
      begin + end, std::move(predicate_name), std::move(argument_list));
}

std::unique_ptr<ast::Conjunction> Parser::parse_conjunction() {
  LOG_DEBUG(parser_logger, "Parsing conjunction");
  const auto begin = lexer_.location();
  advance();
  auto arguments = std::make_unique<std::vector<ast::Condition>>();
  while (skip_if<token::LParen>()) {
    skip_comments();
    auto argument = parse_condition();
    arguments->push_back(std::move(argument));
    skip<token::RParen>();
    skip_comments();
  }
  const auto &end = lexer_.location();
  LOG_DEBUG(parser_logger, "End of conjunction (%u element(s))", arguments->size());
  auto condition_list =
      std::make_unique<ast::ConditionList>(begin + end, std::move(arguments));
  return std::make_unique<ast::Conjunction>(condition_list->location,
                                            std::move(condition_list));
}

std::unique_ptr<ast::Disjunction> Parser::parse_disjunction() {
  LOG_DEBUG(parser_logger, "Parsing disjunction");
  const auto begin = lexer_.location();
  advance();
  auto arguments = std::make_unique<std::vector<ast::Condition>>();
  while (skip_if<token::LParen>()) {
    skip_comments();
    auto argument = parse_condition();
    arguments->push_back(std::move(argument));
    skip<token::RParen>();
    skip_comments();
  }
  const auto &end = lexer_.location();
  LOG_DEBUG(parser_logger, "End of disjunction (%u element(s))", arguments->size());
  auto condition_list =
      std::make_unique<ast::ConditionList>(begin + end, std::move(arguments));
  return std::make_unique<ast::Disjunction>(condition_list->location,
                                            std::move(condition_list));
}

ast::Condition Parser::parse_condition() {
  if (lexer_.has_type<token::Name>() || lexer_.has_type<token::Equality>()) {
    return parse_predicate_evaluation();
  } else if (lexer_.has_type<token::And>()) {
    return parse_conjunction();
  } else if (lexer_.has_type<token::Or>()) {
    return parse_disjunction();
  } else if (lexer_.has_type<token::Not>()) {
    LOG_DEBUG(parser_logger, "Parsing negation");
    const auto begin = lexer_.location();
    advance();
    skip<token::LParen>();
    skip_comments();
    auto condition = parse_condition();
    skip<token::RParen>();
    skip_comments();
    const auto &end = lexer_.location();
    LOG_DEBUG(parser_logger, "End of negation");
    return std::make_unique<ast::Negation>(begin + end, std::move(condition));
  } else if (lexer_.has_type<token::Increase>() ||
             lexer_.has_type<token::Decrease>()) {
    int count = 0;
    while (count >= 0) {
      advance();
      if (lexer_.has_type<token::LParen>()) {
        ++count;
      } else if (lexer_.has_type<token::RParen>()) {
        --count;
      }
    }
  } else {
    LOG_WARN(parser_logger, "Parsing empty condition");
  }
  return std::make_unique<ast::Empty>(lexer_.location());
}

std::unique_ptr<ast::ConditionList> Parser::parse_init_list() {
  LOG_DEBUG(parser_logger, "Parsing init list");
  const auto begin = lexer_.location();
  auto arguments = std::make_unique<std::vector<ast::Condition>>();
  while (skip_if<token::LParen>()) {
    skip_comments();
    if (lexer_.has_type<token::Not>()) {
      LOG_DEBUG(parser_logger, "Parsing negation");
      const auto begin_negation = lexer_.location();
      advance();
      skip<token::LParen>();
      skip_comments();
      auto argument = parse_predicate_evaluation();
      skip<token::RParen>();
      skip_comments();
      const auto &end = lexer_.location();
      auto negation = std::make_unique<ast::Negation>(begin_negation + end,
                                                      std::move(argument));
      arguments->push_back(std::move(negation));
      LOG_DEBUG(parser_logger, "End of negation");
    } else if (lexer_.has_type<token::Equality>()) {
      int count = 0;
      while (count >= 0) {
        advance();
        if (lexer_.has_type<token::LParen>()) {
          ++count;
        } else if (lexer_.has_type<token::RParen>()) {
          --count;
        }
      }
    } else {
      auto argument = parse_predicate_evaluation();
      arguments->push_back(std::move(argument));
    }
    skip<token::RParen>();
    skip_comments();
  }
  const auto &end = lexer_.location();
  LOG_DEBUG(parser_logger, "End of init list (%u element(s))", arguments->size());
  return std::make_unique<ast::ConditionList>(begin + end,
                                              std::move(arguments));
}

std::unique_ptr<ast::ActionDef> Parser::parse_action() {
  LOG_DEBUG(parser_logger, "Parsing action definition");
  const auto begin = lexer_.location();
  lexer_.next();
  const auto &name = lexer_.get<token::Name>().name;
  LOG_DEBUG(parser_logger, "Found action name \'%s\'", std::string{name}.c_str());
  auto action_name =
      std::make_unique<ast::Identifier>(lexer_.location(), std::move(name));
  advance();
  skip<token::Parameters>();
  skip_comments();
  skip<token::LParen>();
  skip_comments();
  auto parameters = parse_typed_variable_list();
  skip<token::RParen>();
  skip_comments();
  std::unique_ptr<ast::Precondition> precondition = nullptr;
  if (lexer_.has_type<token::Precondition>()) {
    LOG_DEBUG(parser_logger, "Parsing precondition");
    const auto precondition_begin = lexer_.location();
    advance();
    skip<token::LParen>();
    skip_comments();
    auto condition = parse_condition();
    skip<token::RParen>();
    const auto end = lexer_.location();
    skip_comments();
    precondition = std::make_unique<ast::Precondition>(precondition_begin + end,
                                                       std::move(condition));
  }
  std::unique_ptr<ast::Effect> effect = nullptr;
  if (lexer_.has_type<token::Effect>()) {
    LOG_DEBUG(parser_logger, "Parsing effect");
    const auto effect_begin = lexer_.location();
    advance();
    skip<token::LParen>();
    skip_comments();
    auto condition = parse_condition();
    skip<token::RParen>();
    const auto end = lexer_.location();
    skip_comments();
    effect =
        std::make_unique<ast::Effect>(effect_begin + end, std::move(condition));
  }
  const auto &end = lexer_.location();
  LOG_DEBUG(parser_logger, "End of action definition");
  return std::make_unique<ast::ActionDef>(
      begin + end, std::move(action_name), std::move(parameters),
      std::move(precondition), std::move(effect));
}

std::unique_ptr<ast::ObjectsDef> Parser::parse_objects() {
  LOG_DEBUG(parser_logger, "Parsing objects definition");
  const auto begin = lexer_.location();
  advance();
  auto objects = parse_typed_identifier_list();
  const auto &end = objects->location;
  LOG_DEBUG(parser_logger, "End of objects definition");
  return std::make_unique<ast::ObjectsDef>(begin + end, std::move(objects));
}

std::unique_ptr<ast::InitDef> Parser::parse_init() {
  LOG_DEBUG(parser_logger, "Parsing init definition");
  const auto begin = lexer_.location();
  advance();
  auto init_list = parse_init_list();
  const auto &end = lexer_.location();
  LOG_DEBUG(parser_logger, "End of init definition");
  return std::make_unique<ast::InitDef>(begin + end, std::move(init_list));
}

std::unique_ptr<ast::GoalDef> Parser::parse_goal() {
  LOG_DEBUG(parser_logger, "Parsing goal definition");
  const auto begin = lexer_.location();
  advance();
  skip<token::LParen>();
  skip_comments();
  auto condition = parse_condition();
  skip<token::RParen>();
  const auto &end = lexer_.location();
  LOG_DEBUG(parser_logger, "End of goal definition");
  return std::make_unique<ast::GoalDef>(begin + end, std::move(condition));
}

// TODO function support
std::unique_ptr<ast::FunctionsDef> Parser::parse_functions() {
  LOG_WARN(parser_logger,
           "Functions will be ignored and have limited parsing support");
  LOG_DEBUG(parser_logger, "Parsing functions definition");
  const auto begin = lexer_.location();
  int count = 0;
  while (count >= 0) {
    advance();
    if (lexer_.has_type<token::LParen>()) {
      ++count;
    } else if (lexer_.has_type<token::RParen>()) {
      --count;
    }
  }
  LOG_DEBUG(parser_logger, "End of functions definition");
  const auto &end = lexer_.location();
  return std::make_unique<ast::FunctionsDef>(begin + end);
}

// TODO metric support
std::unique_ptr<ast::MetricDef> Parser::parse_metric() {
  LOG_WARN(parser_logger, "Metrics will be ignored and have limited parsing support");
  LOG_DEBUG(parser_logger, "Parsing metric definition");
  const auto begin = lexer_.location();
  int count = 0;
  while (count >= 0) {
    advance();
    if (lexer_.has_type<token::LParen>()) {
      ++count;
    } else if (lexer_.has_type<token::RParen>()) {
      --count;
    }
  }
  LOG_DEBUG(parser_logger, "End of metric definition");
  const auto &end = lexer_.location();
  return std::make_unique<ast::MetricDef>(begin + end);
}

std::unique_ptr<ast::Domain> Parser::parse_domain() {
  LOG_DEBUG(parser_logger, "Parsing domain");
  const auto begin = lexer_.location();
  skip<token::LParen>();
  skip<token::Define>();
  skip<token::LParen>();
  skip<token::Domain>();
  const auto &name = lexer_.get<token::Name>().name;
  LOG_DEBUG(parser_logger, "Found domain name \'%s\'", std::string{name}.c_str());
  auto domain_name = std::make_unique<ast::Identifier>(lexer_.location(), name);
  lexer_.next();
  skip<token::RParen>();
  skip_comments();
  auto domain_body = parse_elements<true>();
  skip<token::RParen>();
  const auto &end = lexer_.location();
  LOG_DEBUG(parser_logger, "End of domain");
  return std::make_unique<ast::Domain>(begin + end, std::move(domain_name),
                                       std::move(domain_body));
}

std::unique_ptr<ast::Problem> Parser::parse_problem() {
  LOG_DEBUG(parser_logger, "Parsing problem");
  const auto begin = lexer_.location();
  skip<token::LParen>();
  skip<token::Define>();
  skip<token::LParen>();
  skip<token::Problem>();
  const auto &name = lexer_.get<token::Name>().name;
  LOG_DEBUG(parser_logger, "Found problem name \'%s\'", std::string{name}.c_str());
  auto problem_name =
      std::make_unique<ast::Identifier>(lexer_.location(), name);
  lexer_.next();
  skip<token::RParen>();
  skip<token::LParen>();
  skip<token::DomainRef>();
  const auto &domain_ref = lexer_.get<token::Name>().name;
  LOG_DEBUG(parser_logger, "Found domain reference \'%s\'",
            std::string{domain_ref}.c_str());
  auto domain_ref_name =
      std::make_unique<ast::Identifier>(lexer_.location(), domain_ref);
  lexer_.next();
  skip<token::RParen>();
  skip_comments();
  auto problem_body = parse_elements<false>();
  skip<token::RParen>();
  const auto &end = lexer_.location();
  LOG_DEBUG(parser_logger, "End of problem");
  return std::make_unique<ast::Problem>(begin + end, std::move(problem_name),
                                        std::move(domain_ref_name),
                                        std::move(problem_body));
}

void Parser::parse_domain(ast::AST &ast) {
  skip_comments();
  auto domain = parse_domain();
  ast.set_domain(std::move(domain));
}

void Parser::parse_problem(ast::AST &ast) {
  skip_comments();
  auto problem = parse_problem();
  ast.set_problem(std::move(problem));
}

} // namespace pddl
