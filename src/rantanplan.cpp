#include "parser/lexer.hpp"
#include "parser/location.hpp"
#include "parser/rules.hpp"
#include <cctype>
#include <iostream>
#include <string>
#include <variant>

struct DetectNumber {
  using token_type = int;
  bool matches(const std::vector<char> &current, char c) {
    std::cout << "Check number for " << c << '\n';
    if (std::isdigit(c) || (current.size() == 0 && (c == '+' || c == '-'))) {
      return true;
    }
    return false;
  }
  token_type get_token(const std::vector<char> &current,
                       lexer::Location location) {
    return std::atoi(std::string(current.begin(), current.end()).c_str());
  }
};

struct TokenString {
  TokenString(std::string input, lexer::Location location)
      : input{input}, location{location} {}
  std::string input;
  lexer::Location location;
};

std::ostream &operator<<(std::ostream &out, const TokenString &ts) {
  out << "There is '" << ts.input << "' written at ";
  out << ts.location;
  return out;
}

struct DetectString {
  using token_type = TokenString;
  bool matches(const std::vector<char> &current, char c) {
    std::cout << "Check string for " << c << '\n';
    return std::isalpha(c);
  }
  token_type get_token(const std::vector<char> &current,
                       lexer::Location location) {
    return token_type(std::string(current.begin(), current.end()), location);
  }
};

struct DetectAs {
  using token_type = char;
  bool matches(const std::vector<char> &current, char c) {
    std::cout << "Check As for " << c << '\n';
    return c == 'a';
  }
  token_type get_token(const std::vector<char> &s, lexer::Location location) {
    return s[0];
  }
};

int main(int argc, char *argv[]) {
  std::string input = "\
This1b1is+1a-test+aaa10-10aaab0baaa-abab+b";
  std::cout << input << '\n';
  lexer::Lexer<lexer::Rules<DetectAs, DetectString, DetectNumber>> lexer;
  try {
    std::string name = "test";
    auto vector = lexer.lex(name, input.begin(), input.end());
    std::cout << "Lexing complete!" << std::endl;
    for (auto x : vector) {
      std::visit([](auto t) { std::cout << t << '\n'; }, x);
    }
  } catch (const lexer::LexerException &e) {
    if (e.location()) {
      std::cerr << *e.location();
      std::cerr << ": ";
    }
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
