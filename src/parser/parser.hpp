#ifndef PARSER_HPP
#define PARSER_HPP

#include "lexer/lexer.hpp"

#include <string>
#include <vector>

namespace parser {

namespace tokens {

struct Comment {
  std::string content;
};

} // namespace tokens

namespace rules {

struct Comment {
  using TokenType = tokens::Comment;
  static bool matches(const std::vector<char> &current_string, char c) {
    return !current_string.empty() || c == ';';
  }
  static TokenType get_token(const std::vector<char> &current_string) {
    tokens::Comment comment;
    comment.content = std::string{current_string.begin() + 1, current_string.end()};
    return comment;
  }
};

struct Else {
  using TokenType = std::string;
  static bool matches(const std::vector<char> &current_string, char c) {
    return c != ';';
  }
  static TokenType get_token(const std::vector<char> &current_string) {
    return std::string{current_string.begin(), current_string.end()};
  }
};

} // namespace rules

} // namespace parser

#endif /* end of include guard: PARSER_HPP */
