#include "config.hpp"
#include "lexer/lexer.hpp"
#include "lexer/matcher.hpp"
#include "parser/parser.hpp"

#include <cctype>
#include <iostream>
#include <string>
#include <variant>

int main(int argc, char *argv[]) {
  if constexpr (debug_mode) {
    std::cout << "Rantanplan v" << VERSION_MAJOR << '.' << VERSION_MINOR
              << " DEBUG build" << '\n';
  } else {
    std::cout << "Rantanplan v" << VERSION_MAJOR << '.' << VERSION_MINOR
              << '\n';
  }
  std::string input = "Dies ist eine PDDL\n\
 Dies ist eine zweite Zeile ;;mitKommentar\n\
; Full line Kommentar\n\
Noch ein ;Kommentar";
  std::cout << input << '\n';
  lexer::Lexer<lexer::Matcher<parser::rules::Comment, parser::rules::Else>>
      lexer;
  std::string name = "test";
  auto tokens = lexer.lex(name, input.begin(), input.end());
  try {
    while (!tokens.end()) {
      if (auto t = std::get_if<parser::tokens::Comment>(&*tokens)) {
        std::cout << "Comment at " << tokens.location() << ": " << t->content
                  << '\n';
      }
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
