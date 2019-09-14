#include "config.hpp"
#include "encoding/encoding.hpp"
#include "lexer/lexer.hpp"
#include "lexer/rule_set.hpp"
#include "model/model.hpp"
#include "model/normalize.hpp"
#include "parser/ast.hpp"
#include "parser/parser.hpp"
#include "parser/parser_exception.hpp"
#include "parser/pddl_visitor.hpp"
#include "sat/ipasir_solver.hpp"
#include "util/logger.hpp"
#include "util/option_parser.hpp"

#include <cctype>
#include <chrono>
#include <climits>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <sstream>
#include <string>
#include <unistd.h>
#include <variant>

using namespace std::chrono_literals;

static void print_version() {
  char hostname[HOST_NAME_MAX];
  gethostname(hostname, HOST_NAME_MAX);
  const char *format = "Rantanplan v%u.%u %srunning on %s";
  PRINT_INFO(format, VERSION_MAJOR, VERSION_MINOR,
             debug_mode ? "DEBUG build " : "", hostname);
}

int main(int argc, char *argv[]) {
  print_version();

  Config config;

  options::Options options{};

  options.add_positional_option<std::string>("domain");
  options.add_positional_option<std::string>("problem");
  options.add_option<bool>("log-parser", "p", "Enable logging for the parser",
                           "0");
  options.add_option<bool>("log-encoding", "e",
                           "Enable logging for the encoder", "0");
  options.add_option<bool>("log-visitor", "v",
                           "Enable logging for the ast visitor", "0");
  options.parse(argc, argv);
  config.domain_file = options.get<std::string>("domain");
  config.problem_file = options.get<std::string>("problem");
  config.log_parser = options.get<bool>("log-parser");
  config.log_encoding = options.get<bool>("log-encoding");
  config.log_visitor = options.get<bool>("log-visitor");

  if (config.log_parser) {
    parser::logger.add_default_appender();
  }
  if (config.log_encoding) {
    encoding::logger.add_default_appender();
  }

  std::ifstream domain(config.domain_file);
  std::ifstream problem(config.problem_file);

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
        lexer.lex(config.domain_file, std::istreambuf_iterator<char>(domain),
                  std::istreambuf_iterator<char>());
    PRINT_INFO("Parsing problem...");
    parser::parse_domain(domain_tokens, ast);
    auto problem_tokens =
        lexer.lex(config.problem_file, std::istreambuf_iterator<char>(problem),
                  std::istreambuf_iterator<char>());

    parser::parse_problem(problem_tokens, ast);
    parser::PddlAstParser visitor;
    auto abstract_problem = visitor.parse(ast);
    PRINT_INFO("Normalizing problem...");
    auto problem = model::normalize(abstract_problem);
    /* std::cout << problem; */
    encoding::Encoder encoder{problem};
    encoder.encode();
  } catch (const parser::ParserException &e) {
    std::stringstream ss;
    if (e.location()) {
      ss << *e.location();
      ss << ": ";
    }
    ss << e.what();
    PRINT_ERROR(ss.str().c_str());
    return 1;
  } catch (const lexer::LexerException &e) {
    std::stringstream ss;
    if (e.location()) {
      ss << *e.location();
      ss << ": ";
    }
    ss << e.what();
    PRINT_ERROR(ss.str().c_str());
    return 1;
  }

  return 0;
}
