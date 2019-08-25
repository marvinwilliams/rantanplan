#include "config.hpp"
#include "lexer/lexer.hpp"
#include "lexer/matcher.hpp"
#include "parser/parser.hpp"

#include <cctype>
#include <fstream>
#include <iostream>
#include <iterator>
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
  std::ifstream input(argv[1]);
  std::string filename(argv[1]);
  /* std::string input = "Dies ist eine PDDL\n\ */
  /* Dies ist eine zweite Zeile ;;mitKommentar\n\ */
  /* ; Full line Kommentar\n\ */
  /* Noch ein ;Kommentar"; */
  /* std::cout << input << '\n'; */
  using matching =
      lexer::Matcher<parser::rules::Primitive<parser::tokens::LParen>,
                     parser::rules::Primitive<parser::tokens::RParen>,
                     parser::rules::Primitive<parser::tokens::Hyphen>,
                     parser::rules::Primitive<parser::tokens::Equality>,
                     parser::rules::Primitive<parser::tokens::And>,
                     parser::rules::Primitive<parser::tokens::Or>,
                     parser::rules::Primitive<parser::tokens::Not>,
                     parser::rules::Primitive<parser::tokens::Define>,
                     parser::rules::Primitive<parser::tokens::Domain>,
                     parser::rules::Primitive<parser::tokens::Problem>,
                     parser::rules::Section, parser::rules::Identifier,
                     parser::rules::Variable, parser::rules::Comment>;
  lexer::Lexer<matching> lexer;
  auto tokens = lexer.lex(filename, std::istreambuf_iterator<char>(input),
                          std::istreambuf_iterator<char>());
  try {
    while (!tokens.end()) {
      /* if (auto t = std::get_if<parser::tokens::LParen>(&*tokens)) { */
      /*   std::cout << tokens.location() << ": " */
      /*             << "LParen" << '\n'; */
      /* } else if (auto t = std::get_if<parser::tokens::RParen>(&*tokens)) { */
      /*   std::cout << tokens.location() << ": " */
      /*             << "RParen" << '\n'; */
      /* } else if (auto t = std::get_if<parser::tokens::Hyphen>(&*tokens)) { */
      /*   std::cout << tokens.location() << ": " */
      /*             << "Hyphen" << '\n'; */
      /* } else if (auto t = std::get_if<parser::tokens::Equality>(&*tokens)) { */
      /*   std::cout << tokens.location() << ": " */
      /*             << "Equality" << '\n'; */
      /* } else if (auto t = std::get_if<parser::tokens::Comment>(&*tokens)) { */
      /*   std::cout << tokens.location() << ": " << t->content << '\n'; */
      if (auto t = std::get_if<parser::tokens::Section>(&*tokens)) {
        std::cout << tokens.location() << ": Section " << t->name << '\n';
      } else if (auto t = std::get_if<parser::tokens::Variable>(&*tokens)) {
        std::cout << tokens.location() << ": Variable " << t->name << '\n';
      } else if (auto t = std::get_if<parser::tokens::Identifier>(&*tokens)) {
        std::cout << tokens.location() << ": Identifier " << t->name << '\n';
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
