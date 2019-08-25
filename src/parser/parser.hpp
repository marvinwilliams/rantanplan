#ifndef PARSER_HPP
#define PARSER_HPP

#include "lexer/lexer.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace parser {

namespace tokens {

struct LParen {
  inline static const std::string primitive = "(";
};
struct RParen {
  inline static const std::string primitive = ")";
};
struct Hyphen {
  inline static const std::string primitive = "-";
};
struct Equality {
  inline static const std::string primitive = "=";
};
struct And {
  inline static const std::string primitive = "and";
};
struct Or {
  inline static const std::string primitive = "or";
};
struct Not {
  inline static const std::string primitive = "Not";
};
struct Define {
  inline static const std::string primitive = "define";
};
struct Domain {
  inline static const std::string primitive = "domain";
};
struct Problem {
  inline static const std::string primitive = "problem";
};
struct Section {
  std::string name;
};
struct Identifier {
  std::string name;
};
struct Variable {
  std::string name;
};
struct Comment {
  std::string content;
};

} // namespace tokens

namespace rules {

template <typename T> struct Primitive {
  using TokenType = T;
  static bool matches(const std::vector<char> &current_string, char c) {
    return (current_string.size() < T::primitive.size())
               ? T::primitive[current_string.size()] == c
               : false;
  }
  static TokenType get_token(const std::vector<char> &) {
    static const T primitive = T{};
    return primitive;
  }
};

struct Section {
  using TokenType = tokens::Section;
  static bool matches(const std::vector<char> &current_string, char c) {
    if (current_string.empty()) {
      return c == ':';
    }
    return std::isalpha(c);
  }
  static TokenType get_token(const std::vector<char> &current_string) {
    tokens::Section section;
    section.name =
        std::string{current_string.begin() + 1, current_string.end()};
    return section;
  }
};

struct Identifier {
  using TokenType = tokens::Identifier;
  static bool matches(const std::vector<char> &current_string, char c) {
    return std::isalnum(c) || c == '_';
  }
  static TokenType get_token(const std::vector<char> &current_string) {
    tokens::Identifier identifier;
    identifier.name = std::string{current_string.begin(), current_string.end()};
    return identifier;
  }
};

struct Variable {
  using TokenType = tokens::Variable;
  static bool matches(const std::vector<char> &current_string, char c) {
    if (current_string.empty()) {
      return c == '?';
    }
    return std::isalnum(c) || c == '_';
  }
  static TokenType get_token(const std::vector<char> &current_string) {
    tokens::Variable variable;
    variable.name =
        std::string{current_string.begin() + 1, current_string.end()};
    return variable;
  }
};

struct Comment {
  using TokenType = tokens::Comment;
  static bool matches(const std::vector<char> &current_string, char c) {
    return !current_string.empty() || c == ';';
  }
  static TokenType get_token(const std::vector<char> &current_string) {
    tokens::Comment comment;
    comment.content = std::string{current_string.begin(), current_string.end()};
    return comment;
  }
};

} // namespace rules

} // namespace parser

#endif /* end of include guard: PARSER_HPP */
