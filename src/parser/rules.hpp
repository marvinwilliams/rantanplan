#ifndef RULES_HPP
#define RULES_HPP

#include "parser/tokens.hpp"

#include <cctype>
#include <cstring>
#include <string>

namespace parser {

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
    variable.name =
        std::string{current_string.begin() + 1, current_string.end()};
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

} // namespace parser

#endif /* end of include guard: RULES_HPP */
