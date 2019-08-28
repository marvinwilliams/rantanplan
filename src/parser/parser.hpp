#ifndef PARSER_HPP
#define PARSER_HPP

#include "parser/ast.hpp"
#include "parser/parser_exception.hpp"

#include <iostream>
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace parser {

namespace tokens {

struct LParen {
  static constexpr auto printable_name = "(";
  static constexpr auto primitive = "(";
};
struct RParen {
  static constexpr auto printable_name = ")";
  static constexpr auto primitive = ")";
};
struct Hyphen {
  static constexpr auto printable_name = "-";
  static constexpr auto primitive = "-";
};
struct Equality {
  static constexpr auto printable_name = "=";
  static constexpr auto primitive = "=";
};
struct And {
  static constexpr auto printable_name = "and";
  static constexpr auto primitive = "and";
};
struct Or {
  static constexpr auto printable_name = "or";
  static constexpr auto primitive = "or";
};
struct Not {
  static constexpr auto printable_name = "not";
  static constexpr auto primitive = "not";
};
struct Define {
  static constexpr auto printable_name = "define";
  static constexpr auto primitive = "define";
};
struct Domain {
  static constexpr auto printable_name = "domain";
  static constexpr auto primitive = "domain";
};
struct Problem {
  static constexpr auto printable_name = "problem";
  static constexpr auto primitive = "problem";
};
struct Section {
  static constexpr auto printable_name = "Section";
  std::string name;
};
struct Identifier {
  static constexpr auto printable_name = "Identifier";
  std::string name;
};
struct Variable {
  static constexpr auto printable_name = "Variable";
  std::string name;
};
struct Comment {
  static constexpr auto printable_name = "Comment";
  std::string content;
};

} // namespace tokens

namespace rules {

template <typename T> struct Primitive {
  using TokenType = T;
  bool accepts(char c) {
    if (valid && index < std::strlen(T::primitive)) {
      valid = T::primitive[index] == c;
    }
    index++;
    return (valid && index < std::strlen(T::primitive));
  }
  bool matches() const { return valid && index == std::strlen(T::primitive); }
  TokenType get_token(const std::string &) const {
    static const T primitive = T{};
    return primitive;
  }
  void reset() {
    valid = true;
    index = 0;
  }
  bool valid = true;
  unsigned int index = 0;
};

struct Section {
  using TokenType = tokens::Section;
  bool accepts(char c) {
    if (index == 0) {
      index++;
      return c == ':';
    }
    if (index == 1) {
      valid = std::isalpha(c);
    } else {
      valid = std::isalnum(c) || c == '_' || c == '-';
    }
    index++;
    return valid;
  }
  bool matches() const { return valid; }
  TokenType get_token(const std::string &current_string) const {
    tokens::Section section;
    section.name =
        std::string{current_string.begin() + 1, current_string.end()};
    return section;
  }
  void reset() {
    valid = false;
    index = 0;
  }
  bool valid = false;
  unsigned int index = 0;
};

struct Identifier {
  using TokenType = tokens::Identifier;
  bool accepts(char c) {
    if (first) {
      valid = std::isalpha(c);
      first = false;
    } else if (valid) {
      valid = std::isalnum(c) || c == '_' || c == '-';
    }
    return valid;
  }
  bool matches() const { return valid; }
  TokenType get_token(const std::string &current_string) const {
    tokens::Identifier identifier;
    identifier.name = current_string;
    return identifier;
  }
  void reset() {
    valid = false;
    first = true;
  }
  bool valid = false;
  bool first = true;
};

struct Variable {
  using TokenType = tokens::Variable;
  bool accepts(char c) {
    if (index == 0) {
      index++;
      return c == '?';
    }
    if (index == 1) {
      valid = std::isalpha(c);
    } else {
      valid = std::isalnum(c) || c == '_' || c == '-';
    }
    index++;
    return valid;
  }
  bool matches() const { return valid; }
  TokenType get_token(const std::string &current_string) const {
    tokens::Variable variable;
    variable.name = current_string;
    return variable;
  }
  void reset() {
    valid = false;
    index = 0;
  }
  bool valid = false;
  unsigned int index = 0;
};

struct Comment {
  using TokenType = tokens::Comment;
  bool accepts(char c) {
    if (first) {
      first = false;
      valid = c == ';';
    }
    return valid;
  }
  bool matches() const { return valid; }
  TokenType get_token(const std::string &current_string) const {
    tokens::Comment comment;
    comment.content =
        std::string{current_string.begin() + 1, current_string.end()};
    return comment;
  }
  void reset() {
    valid = false;
    first = true;
  }
  bool valid = false;
  bool first = true;
};

} // namespace rules

template <typename TokenIterator> class Parser {
public:
  Parser(const TokenIterator &token_iterator)
      : token_iterator_(token_iterator) {
    if (has_type<tokens::Comment>()) {
      advance();
    }
  }
  std::unique_ptr<ast::NameList> parse_name_list() {
    auto begin = token_iterator_.location();
    auto names = std::make_unique<std::vector<std::unique_ptr<ast::Name>>>();
    while (has_type<tokens::Identifier>()) {
      auto name = get<tokens::Identifier>().name;
      auto identifier =
          std::make_unique<ast::Name>(token_iterator_.location(), name);
      names->push_back(std::move(identifier));
      advance();
    }
    return std::make_unique<ast::NameList>(begin + token_iterator_.location(),
                                           std::move(names));
  }

  std::unique_ptr<ast::VariableList> parse_variable_list() {
    auto begin = token_iterator_.location();
    auto variables =
        std::make_unique<std::vector<std::unique_ptr<ast::Variable>>>();
    while (has_type<tokens::Variable>()) {
      auto name = get<tokens::Variable>().name;
      auto variable =
          std::make_unique<ast::Variable>(token_iterator_.location(), name);
      variables->push_back(std::move(variable));
      advance();
    }
    return std::make_unique<ast::VariableList>(
        begin + token_iterator_.location(), std::move(variables));
  }

  std::unique_ptr<ast::ArgumentList> parse_argument_list() {
    auto begin = token_iterator_.location();
    auto arguments =
        std::make_unique<std::vector<std::unique_ptr<ast::Argument>>>();
    while (has_type<tokens::Variable>() || has_type<tokens::Identifier>()) {
      if (has_type<tokens::Variable>()) {
        auto name = get<tokens::Variable>().name;
        auto argument = std::make_unique<ast::Argument>(
            ast::Variable{token_iterator_.location(), name});
        arguments->push_back(std::move(argument));
      } else {
        auto name = get<tokens::Identifier>().name;
        auto argument = std::make_unique<ast::Argument>(
            ast::Name{token_iterator_.location(), name});
        arguments->push_back(std::move(argument));
      }
      advance();
    }
    return std::make_unique<ast::ArgumentList>(
        begin + token_iterator_.location(), std::move(arguments));
  }

  std::unique_ptr<ast::TypedNameList> parse_typed_name_list() {
    auto begin = token_iterator_.location();
    auto lists = std::make_unique<
        std::vector<std::unique_ptr<ast::SingleTypedNameList>>>();
    while (has_type<tokens::Identifier>()) {
      auto inner_begin = token_iterator_.location();
      auto name_list = parse_name_list();
      std::optional<ast::Name> type;
      if (skip_if<tokens::Hyphen>()) {
        type = ast::Name{token_iterator_.location(),
                         get<tokens::Identifier>().name};
        advance();
      }
      auto single_list = std::make_unique<ast::SingleTypedNameList>(
          inner_begin + token_iterator_.location(), std::move(name_list), type);
      lists->push_back(std::move(single_list));
    }
    return std::make_unique<ast::TypedNameList>(
        begin + token_iterator_.location(), std::move(lists));
  }

  std::unique_ptr<ast::TypedVariableList> parse_typed_variable_list() {
    auto begin = token_iterator_.location();
    auto lists = std::make_unique<
        std::vector<std::unique_ptr<ast::SingleTypedVariableList>>>();
    while (has_type<tokens::Variable>()) {
      auto inner_begin = token_iterator_.location();
      auto variable_list = parse_variable_list();
      std::optional<ast::Name> type;
      if (skip_if<tokens::Hyphen>()) {
        type = ast::Name{token_iterator_.location(),
                         get<tokens::Identifier>().name};
        advance();
      }
      auto single_list = std::make_unique<ast::SingleTypedVariableList>(
          inner_begin + token_iterator_.location(), std::move(variable_list),
          type);
      lists->push_back(std::move(single_list));
    }
    return std::make_unique<ast::TypedVariableList>(
        begin + token_iterator_.location(), std::move(lists));
  }

  std::unique_ptr<ast::RequirementList> parse_requirement_list() {
    auto begin = token_iterator_.location();
    auto requirements =
        std::make_unique<std::vector<std::unique_ptr<ast::Requirement>>>();
    while (has_type<tokens::Section>()) {
      auto name = get<tokens::Section>().name;
      auto requirement =
          std::make_unique<ast::Requirement>(token_iterator_.location(), name);
      requirements->push_back(std::move(requirement));
      advance();
    }
    return std::make_unique<ast::RequirementList>(
        begin + token_iterator_.location(), std::move(requirements));
  }

  std::unique_ptr<ast::Element> parse_requirements() {
    auto begin = token_iterator_.location();
    advance();
    auto requirement_list = parse_requirement_list();
    return std::make_unique<ast::Element>(ast::RequirementsDef{
        begin + token_iterator_.location(), std::move(requirement_list)});
  }

  std::unique_ptr<ast::Element> parse_types() {
    auto begin = token_iterator_.location();
    advance();
    auto type_list = parse_typed_name_list();
    return std::make_unique<ast::Element>(ast::TypesDef{
        begin + token_iterator_.location(), std::move(type_list)});
  }

  std::unique_ptr<ast::Element> parse_constants() {
    auto begin = token_iterator_.location();
    advance();
    auto constant_list = parse_typed_name_list();
    return std::make_unique<ast::Element>(ast::ConstantsDef{
        begin + token_iterator_.location(), std::move(constant_list)});
  }

  std::unique_ptr<ast::PredicateList> parse_predicate_list() {
    auto begin = token_iterator_.location();
    auto predicates =
        std::make_unique<std::vector<std::unique_ptr<ast::Predicate>>>();
    while (skip_if<tokens::LParen>()) {
      auto inner_begin = token_iterator_.location();
      auto name = get<tokens::Identifier>().name;
      auto identifier =
          std::make_unique<ast::Name>(token_iterator_.location(), name);
      advance();
      auto parameters = parse_typed_variable_list();
      auto predicate = std::make_unique<ast::Predicate>(
          inner_begin + token_iterator_.location(), std::move(identifier),
          std::move(parameters));
      predicates->push_back(std::move(predicate));
      skip<tokens::RParen>();
    }
    return std::make_unique<ast::PredicateList>(
        begin + token_iterator_.location(), std::move(predicates));
  }

  std::unique_ptr<ast::Element> parse_predicates() {
    auto begin = token_iterator_.location();
    advance();
    auto predicate_list = parse_predicate_list();
    return std::make_unique<ast::Element>(ast::PredicatesDef{
        begin + token_iterator_.location(), std::move(predicate_list)});
  }

  template <bool is_precondition>
  std::unique_ptr<ast::Condition> parse_condition() {
    auto begin = token_iterator_.location();
    if (has_type<tokens::Identifier>()) {
      auto name = std::make_unique<ast::Name>(token_iterator_.location(),
                                              get<tokens::Identifier>().name);
      advance();
      auto argument_list = parse_argument_list();
      return std::make_unique<ast::Condition>(
          ast::PredicateEvaluation{begin + token_iterator_.location(),
                                   std::move(name), std::move(argument_list)});
    } else if (is_precondition && skip_if<tokens::Equality>()) {
      auto name = std::make_unique<ast::Name>(token_iterator_.location(), "=");
      auto argument_list = parse_argument_list();
      return std::make_unique<ast::Condition>(
          ast::PredicateEvaluation{begin + token_iterator_.location(),
                                   std::move(name), std::move(argument_list)});
    } else if (skip_if<tokens::And>()) {
      auto inner_begin = token_iterator_.location();
      auto conditions =
          std::make_unique<std::vector<std::unique_ptr<ast::Condition>>>();
      while (skip_if<tokens::LParen>()) {
        auto condition = parse_condition<is_precondition>();
        conditions->push_back(std::move(condition));
        skip<tokens::RParen>();
      }
      auto condition_list = std::make_unique<ast::ConditionList>(
          inner_begin + token_iterator_.location(), std::move(conditions));
      return std::make_unique<ast::Condition>(ast::Conjunction{
          begin + token_iterator_.location(), std::move(condition_list)});
    } else if (is_precondition && skip_if<tokens::Or>()) {
      auto inner_begin = token_iterator_.location();
      auto conditions =
          std::make_unique<std::vector<std::unique_ptr<ast::Condition>>>();
      while (skip_if<tokens::LParen>()) {
        auto condition = parse_condition<is_precondition>();
        conditions->push_back(std::move(condition));
        skip<tokens::RParen>();
      }
      auto condition_list = std::make_unique<ast::ConditionList>(
          inner_begin + token_iterator_.location(), std::move(conditions));
      return std::make_unique<ast::Condition>(ast::Conjunction{
          begin + token_iterator_.location(), std::move(condition_list)});
    } else if (skip_if<tokens::Not>()) {
      skip<tokens::LParen>();
      auto condition = parse_condition<is_precondition>();
      skip<tokens::RParen>();
      return std::make_unique<ast::Condition>(ast::Negation{
          begin + token_iterator_.location(), std::move(condition)});
    }
    return std::make_unique<ast::Condition>();
  }

  std::unique_ptr<ast::Element> parse_action() {
    auto begin = token_iterator_.location();
    advance();
    auto name = std::make_unique<ast::Name>(token_iterator_.location(),
                                            get<tokens::Identifier>().name);
    advance();
    skip<tokens::Section>();
    skip<tokens::LParen>();
    auto parameters = parse_typed_variable_list();
    skip<tokens::RParen>();
    std::optional<std::unique_ptr<ast::Precondition>> precondition;
    if (has_type<tokens::Section>()) {
      auto section = get<tokens::Section>();
      if (section.name == "precondition") {
        auto inner_begin = token_iterator_.location();
        advance();
        skip<tokens::LParen>();
        auto condition = parse_condition<true>();
        skip<tokens::RParen>();
        precondition = std::make_unique<ast::Precondition>(
            inner_begin + token_iterator_.location(), std::move(condition));
      }
    }
    std::optional<std::unique_ptr<ast::Effect>> effect;
    if (has_type<tokens::Section>()) {
      auto section = get<tokens::Section>();
      if (section.name == "effect") {
        auto inner_begin = token_iterator_.location();
        advance();
        skip<tokens::LParen>();
        auto condition = parse_condition<false>();
        skip<tokens::RParen>();
        effect = std::make_unique<ast::Effect>(
            inner_begin + token_iterator_.location(), std::move(condition));
      }
    }
    return std::make_unique<ast::Element>(ast::ActionDef{
        begin + token_iterator_.location(), std::move(name),
        std::move(parameters), std::move(precondition), std::move(effect)});
  }

  std::unique_ptr<ast::Element> parse_element() {
    auto section = get<tokens::Section>();
    if (section.name == "requirements") {
      return parse_requirements();
    } else if (section.name == "types") {
      return parse_types();
    } else if (section.name == "constants") {
      return parse_constants();
    } else if (section.name == "predicates") {
      return parse_predicates();
    } else if (section.name == "action") {
      return parse_action();
    } else {
      std::string msg = "Unknown section: \"" + section.name + "\"";
      throw ParserException(token_iterator_.location(), msg);
    }
  }

  std::unique_ptr<ast::ElementList> parse_domain_body() {
    auto begin = token_iterator_.location();
    auto elements =
        std::make_unique<std::vector<std::unique_ptr<ast::Element>>>();
    while (skip_if<tokens::LParen>()) {
      elements->push_back(parse_element());
      skip<tokens::RParen>();
    }
    return std::make_unique<ast::ElementList>(
        begin + token_iterator_.location(), std::move(elements));
  }

  std::unique_ptr<ast::Domain> parse_domain() {
    lexer::Location begin = token_iterator_.location();
    skip<tokens::LParen>();
    skip<tokens::Define>();
    skip<tokens::LParen>();
    skip<tokens::Domain>();
    auto name = get<tokens::Identifier>().name;
    auto domain_name =
        std::make_unique<ast::Name>(token_iterator_.location(), name);
    advance();
    skip<tokens::RParen>();
    auto domain_body = parse_domain_body();
    skip<tokens::RParen>();
    auto domain = std::make_unique<ast::Domain>(
        begin + token_iterator_.location(), std::move(domain_name),
        std::move(domain_body));
    return domain;
  }

  ast::AST parse() {
    ast::AST ast;
    auto domain = parse_domain();
    ast.set_domain(std::move(domain));
    return ast;
  }

private:
  template <typename TokenType> void expect() {
    if (!token_iterator_.template has_type<TokenType>()) {
      std::string msg = "Expected token \"" +
                        std::string(TokenType::printable_name) +
                        "\" but got \"" + token_iterator_.to_string() + '\"';
      throw ParserException(token_iterator_.location(), msg);
    }
  }

  template <typename TokenType> void skip() {
    expect<TokenType>();
    advance();
  }

  template <typename TokenType> bool has_type() {
    return (token_iterator_.template has_type<TokenType>());
  }

  template <typename TokenType> bool skip_if() {
    if (token_iterator_.template has_type<TokenType>()) {
      advance();
      return true;
    }
    return false;
  }

  template <typename TokenType> TokenType get() {
    expect<TokenType>();
    return std::get<TokenType>(*token_iterator_);
  }

  void advance() {
    do {
      token_iterator_++;
    } while (has_type<tokens::Comment>());
  }

  TokenIterator token_iterator_;
}; // namespace parser
} // namespace parser

#endif /* end of include guard: PARSER_HPP */
