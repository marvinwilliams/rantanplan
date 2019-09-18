#include "config.hpp"
#include "encoding/encoding.hpp"
#include "lexer/lexer.hpp"
#include "lexer/rule_set.hpp"
#include "logging/logging.hpp"
#include "model/normalize.hpp"
#include "options/options.hpp"
#include "parser/parser.hpp"
#include "parser/pddl_visitor.hpp"

#include <climits>
#include <string>
#include <unistd.h>

void print_version() {
  char hostname[HOST_NAME_MAX];
  gethostname(hostname, HOST_NAME_MAX);
  PRINT_INFO("Rantanplan v%u.%u %srunning on %s", VERSION_MAJOR, VERSION_MINOR,
             DEBUG_MODE ? "debug build " : "", hostname);
}

Config get_config(const options::Options &options) {
  Config config;
  config.domain_file = options.get<std::string>("domain");
  config.problem_file = options.get<std::string>("problem");
  config.log_parser = options.get<bool>("log-parser");
  config.log_encoding = options.get<bool>("log-encoding");
  config.log_visitor = options.get<bool>("log-visitor");

  return config;
}

int main(int argc, char *argv[]) {
  print_version();

  options::Options options;

  options.add_positional_option<std::string>("domain");
  options.add_positional_option<std::string>("problem");
  options.add_option<bool>("log-parser", 'p', "Enable logging for the parser",
                           "1");
  options.add_option<bool>("log-encoding", 'e',
                           "Enable logging for the encoder", "1");
  options.add_option<bool>("log-visitor", 'v',
                           "Enable logging for the ast visitor", "1");
  try {
    options.parse(argc, argv);
  } catch (const options::OptionException &e) {
    PRINT_ERROR(e.what());
    return 1;
  }

  auto config = get_config(options);

  if (config.log_parser) {
    parser::logger.add_appender(logging::default_appender);
  }
  if (config.log_encoding) {
    encoding::logger.add_appender(logging::default_appender);
  }

  std::ifstream domain(config.domain_file);
  std::ifstream problem(config.problem_file);
  if (!domain.is_open()) {
    PRINT_ERROR("Failed to open: %s", config.domain_file.c_str());
    return 1;
  }
  if (!problem.is_open()) {
    PRINT_ERROR("Failed to open: %s", config.problem_file.c_str());
    return 1;
  }

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
    PRINT_DEBUG(to_string(abstract_problem).c_str());
    PRINT_INFO("Normalizing problem...");
    auto problem = model::normalize(abstract_problem);
    PRINT_DEBUG(to_string(problem).c_str());
    encoding::Encoder encoder{problem};
    encoder.encode();
    encoder.plan();
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
