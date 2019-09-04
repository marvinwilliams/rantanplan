#include "config.hpp"
#include "grounding/normalize.hpp"
#include "lexer/lexer.hpp"
#include "lexer/rule_set.hpp"
#include "model/model.hpp"
#include "parser/ast.hpp"
#include "parser/parser.hpp"
#include "parser/parser_exception.hpp"
#include "parser/pddl_visitor.hpp"
#include "sat/ipasir_solver.hpp"

#include <cctype>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <string>
#include <variant>

using namespace std::chrono_literals;

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
    auto abstract_problem = visitor.parse(ast);
    std::cout << abstract_problem;
    auto problem = grounding::normalize(abstract_problem);
    std::cout << problem;
    sat::IpasirSolver solver;
    std::random_device rd;
    std::default_random_engine gen(rd());
    std::uniform_int_distribution<> dis(1, 100000);
    std::uniform_int_distribution<> dis2(5, 50);
    int count = 0;
    int length = dis2(gen);
    for (int i = 0; i < 100000000; ++i) {
      solver << dis(gen);
      if (count == length) {
        count = 0;
        length = dis2(gen);
        solver << sat::EndClause;
      }
      ++count;
    }
    std::cout << "Finished generating" << std::endl;
    auto model = solver.solve(1s);
    if (model) {
      std::cout << "Solved!" << std::endl;
      /* for (size_t i = 1; i < (*model).assignment.size(); ++i) { */
      /*   std::cout << (*model)[i] << '\n'; */
      /* } */
    } else {
      std::cout << "Not solved" << std::endl;
    }
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
