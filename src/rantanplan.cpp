#include "config.hpp"
#include "lexer/lexer.hpp"
#include "lexer/rule_set.hpp"
#include "parser/ast.hpp"
#include "parser/parser.hpp"
#include "parser/parser_exception.hpp"
/* #include "parser/parser.hpp" */

#include <cctype>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <variant>

int main(int, char *argv[]) {
  if constexpr (debug_mode) {
    std::cout << "Rantanplan v" << VERSION_MAJOR << '.' << VERSION_MINOR
              << " DEBUG build" << '\n';
  } else {
    std::cout << "Rantanplan v" << VERSION_MAJOR << '.' << VERSION_MINOR
              << '\n';
  }
  std::ifstream domain(argv[1]);
  std::ifstream problem(argv[2]);
  std::string domain_file(argv[1]);
  std::string problem_file(argv[2]);

  using Rules =
      lexer::RuleSet<parser::rules::Primitive<parser::tokens::LParen>,
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
  lexer::Lexer<Rules> lexer;
  parser::ast::AST ast;
  try {
    auto domain_tokens =
        lexer.lex(domain_file, std::istreambuf_iterator<char>(domain),
                  std::istreambuf_iterator<char>());
    parser::parse_domain(domain_tokens, ast);
    auto problem_tokens =
        lexer.lex(problem_file, std::istreambuf_iterator<char>(problem),
                  std::istreambuf_iterator<char>());

    parser::parse_problem(problem_tokens, ast);
  } catch (const parser::ParserException &e) {
    if (e.location()) {
      std::cerr << *e.location();
      std::cerr << ": ";
    }
    std::cerr << e.what() << std::endl;
  } catch (const lexer::LexerException &e) {
    if (e.location()) {
      std::cerr << *e.location();
      std::cerr << ": ";
    }
  }
  //clang-format off
  /* while (!tokens.end()) { */
  /*     if (auto t = std::get_if<tokens::Comment>(&*tokens)) { */
  /*       std::cout << tokens.location() << ": " << t->content << '\n'; */
  /*     } else if (auto t = std::get_if<tokens::Section>(&*tokens)) { */
  /*       std::cout << tokens.location() << ": Section " << t->name << '\n'; */
  /*     } else if (auto t = std::get_if<tokens::Variable>(&*tokens)) { */
  /*       std::cout << tokens.location() << ": Variable " << t->name << '\n';
   */
  /*     } else if (auto t = std::get_if<tokens::Identifier>(&*tokens)) { */
  /*       std::cout << tokens.location() << ": Identifier " << t->name << '\n';
   */
  /*     } else if (std::get_if<tokens::LParen>(&*tokens)) { */
  /*       std::cout << tokens.location() << " LParen" << '\n'; */
  /*     } else if (std::get_if<tokens::RParen>(&*tokens)) { */
  /*       std::cout << tokens.location() << " RParen" << '\n'; */
  /*     } else if (std::get_if<tokens::Hyphen>(&*tokens)) { */
  /*       std::cout << tokens.location() << " Hyphen" << '\n'; */
  /*     } else if (std::get_if<tokens::Equality>(&*tokens)) { */
  /*       std::cout << tokens.location() << " Equality" << '\n'; */
  /*     } else if (std::get_if<tokens::And>(&*tokens)) { */
  /*       std::cout << tokens.location() << " And" << '\n'; */
  /*     } else if (std::get_if<tokens::Or>(&*tokens)) { */
  /*       std::cout << tokens.location() << " Or" << '\n'; */
  /*     } else if (std::get_if<tokens::Not>(&*tokens)) { */
  /*       std::cout << tokens.location() << " Not" << '\n'; */
  /*     } else if (std::get_if<tokens::Define>(&*tokens)) { */
  /*       std::cout << tokens.location() << " Define" << '\n'; */
  /*     } else if (std::get_if<tokens::Domain>(&*tokens)) { */
  /*       std::cout << tokens.location() << " Domain" << '\n'; */
  /*     } else if (std::get_if<tokens::Problem>(&*tokens)) { */
  /*       std::cout << tokens.location() << " Problem" << '\n'; */
  /*     } */
  /*   try { */
  /*     tokens++; */
  /*   } catch (const lexer::LexerException &e) { */
  /*     if (e.location()) { */
  /*       std::cerr << *e.location(); */
  /*       std::cerr << ": "; */
  /*     } */
  /*     std::cerr << e.what() << std::endl; */
  /*   } */
  /* } */
  //clang-format on
  return 0;
}
