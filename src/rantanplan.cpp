#include "config.hpp"
#include "lexer/lexer.hpp"
#include "lexer/rule_set.hpp"
#include "parser/parser.hpp"

#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <variant>

namespace tokens {

struct LParen {
  static constexpr auto primitive = "(";
};
struct RParen {
  static constexpr auto primitive = ")";
};
struct Hyphen {
  static constexpr auto primitive = "-";
};
struct Equality {
  static constexpr auto primitive = "=";
};
struct And {
  static constexpr auto primitive = "and";
};
struct Or {
  static constexpr auto primitive = "or";
};
struct Not {
  static constexpr auto primitive = "Not";
};
struct Define {
  static constexpr auto primitive = "define";
};
struct Domain {
  static constexpr auto primitive = "domain";
};
struct Problem {
  static constexpr auto primitive = "problem";
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
    index++;
    if (index == 1) {
      return c == ':';
    } else {
      valid = std::isalpha(c);
      return valid;
    }
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

int main(int argc, char *argv[]) {
  if constexpr (debug_mode) {
    std::cout << "Rantanplan v" << VERSION_MAJOR << '.' << VERSION_MINOR
              << " DEBUG build" << '\n';
  } else {
    std::cout << "Rantanplan v" << VERSION_MAJOR << '.' << VERSION_MINOR
              << '\n';
  }
  std::ifstream input(argv[1]);
  std::string filename(argv[1]);

  using Rules = lexer::RuleSet<
      rules::Primitive<tokens::LParen>, rules::Primitive<tokens::RParen>,
      rules::Primitive<tokens::Hyphen>, rules::Primitive<tokens::Equality>,
      rules::Primitive<tokens::And>, rules::Primitive<tokens::Or>,
      rules::Primitive<tokens::Not>, rules::Primitive<tokens::Define>,
      rules::Primitive<tokens::Domain>, rules::Primitive<tokens::Problem>,
      rules::Section, rules::Identifier, rules::Variable, rules::Comment>;
  lexer::Lexer<Rules> lexer;
  auto tokens = lexer.lex(filename, std::istreambuf_iterator<char>(input),
                          std::istreambuf_iterator<char>());
  while (!tokens.end()) {
      if (auto t = std::get_if<tokens::Comment>(&*tokens)) {
        std::cout << tokens.location() << ": " << t->content << '\n';
      } else if (auto t = std::get_if<tokens::Section>(&*tokens)) {
        std::cout << tokens.location() << ": Section " << t->name << '\n';
      } else if (auto t = std::get_if<tokens::Variable>(&*tokens)) {
        std::cout << tokens.location() << ": Variable " << t->name << '\n';
      } else if (auto t = std::get_if<tokens::Identifier>(&*tokens)) {
        std::cout << tokens.location() << ": Identifier " << t->name << '\n';
      } else if (std::get_if<tokens::LParen>(&*tokens)) {
        std::cout << tokens.location() << " LParen" << '\n';
      } else if (std::get_if<tokens::RParen>(&*tokens)) {
        std::cout << tokens.location() << " RParen" << '\n';
      } else if (std::get_if<tokens::Hyphen>(&*tokens)) {
        std::cout << tokens.location() << " Hyphen" << '\n';
      } else if (std::get_if<tokens::Equality>(&*tokens)) {
        std::cout << tokens.location() << " Equality" << '\n';
      } else if (std::get_if<tokens::And>(&*tokens)) {
        std::cout << tokens.location() << " And" << '\n';
      } else if (std::get_if<tokens::Or>(&*tokens)) {
        std::cout << tokens.location() << " Or" << '\n';
      } else if (std::get_if<tokens::Not>(&*tokens)) {
        std::cout << tokens.location() << " Not" << '\n';
      } else if (std::get_if<tokens::Define>(&*tokens)) {
        std::cout << tokens.location() << " Define" << '\n';
      } else if (std::get_if<tokens::Domain>(&*tokens)) {
        std::cout << tokens.location() << " Domain" << '\n';
      } else if (std::get_if<tokens::Problem>(&*tokens)) {
        std::cout << tokens.location() << " Problem" << '\n';
      }
    try {
      tokens++;
    } catch (const lexer::LexerException &e) {
      if (e.location()) {
        std::cerr << *e.location();
        std::cerr << ": ";
      }
      std::cerr << e.what() << std::endl;
    }
  }
  std::cout << "Lexing complete!" << std::endl;
  return 0;
}
