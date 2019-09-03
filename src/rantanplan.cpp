#include "config.hpp"
#include "lexer/lexer.hpp"
#include "lexer/rule_set.hpp"
#include "model/model.hpp"
#include "parser/ast.hpp"
#include "parser/parser.hpp"
#include "parser/parser_exception.hpp"
#include "parser/pddl_visitor.hpp"
#include "grounding/normalize.hpp"

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
    parser::PddlAstParser visitor;
    auto problem = visitor.parse(ast);
    std::cout << problem;
    grounding::normalize(problem);
    std::cout << problem;
  } catch (const parser::ParserException &e) {
    if (e.location()) {
      std::cerr << *e.location();
      std::cerr << ": ";
    }
    std::cerr << e.what() << std::endl;
    return 1;
  } catch (const lexer::LexerException &e) {
    if (e.location()) {
      std::cerr << *e.location();
      std::cerr << ": ";
    }
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
