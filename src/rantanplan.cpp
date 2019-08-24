#include "config.hpp"
#include "lexer/lexer.hpp"
#include "lexer/matcher.hpp"

#include <cctype>
#include <iostream>
#include <string>
#include <variant>

struct DetectNumber {
  using TokenType = int;
  static bool matches(const std::vector<char> &current, char c) {
    /* std::cout << "Check number for " << c << '\n'; */
    if (std::isdigit(c) || (current.size() == 0 && (c == '+' || c == '-'))) {
      return true;
    }
    return false;
  }
  static TokenType get_token(const std::vector<char> &current) {
    return std::atoi(std::string(current.begin(), current.end()).c_str());
  }
};

struct TokenString {
  TokenString(std::string input) : input{input} {}
  std::string input;
};

std::ostream &operator<<(std::ostream &out, const TokenString &ts) {
  out << ts.input;
  return out;
}

struct DetectString {
  using TokenType = TokenString;
  static bool matches(const std::vector<char> &, char c) {
    /* std::cout << "Check string for " << c << '\n'; */
    return std::isalpha(c);
  }
  static TokenType get_token(const std::vector<char> &current) {
    return TokenType(std::string(current.begin(), current.end()));
  }
};

struct DetectAs {
  using TokenType = char;
  static bool matches(const std::vector<char> &, char c) {
    /* std::cout << "Check As for " << c << '\n'; */
    return c == 'a';
  }
  static TokenType get_token(const std::vector<char> &s) { return s[0]; }
};

std::ostream &operator<<(std::ostream &out, lexer::ErrorToken) {
  out << "This should never happen" << '\n';
  return out;
}

int main(int argc, char *argv[]) {
  std::cout << "Rantanplan v" << VERSION_MAJOR << '.' << VERSION_MINOR << '\n';
  std::string input = " This  1b\n 1is +1a-test  _[  +a{aa10 \n-10a\naab  "
                      "\n0ba  \n  aa-abab+b";
  std::cout << input << '\n';
  lexer::Lexer<lexer::Matcher<DetectAs, DetectString, DetectNumber>> lexer;
  std::string name = "test";
  auto tokens = lexer.lex(name, input.begin(), input.end());
  try {
    while (!tokens.end()) {
      std::cout << tokens.location() << ": ";
      std::visit([](auto t) { std::cout << t << '\n'; }, *tokens);
      tokens++;
    }
    std::cout << "Lexing complete!" << std::endl;
  } catch (const lexer::LexerException &e) {
    if (e.location()) {
      std::cerr << *e.location();
      std::cerr << ": ";
    }
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
