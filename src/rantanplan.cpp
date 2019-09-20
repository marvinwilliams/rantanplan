#include "config.hpp"
#include "encoding/encoding.hpp"
#include "encoding/foreach.hpp"
#include "encoding/sequential.hpp"
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

void print_version() noexcept {
  char hostname[HOST_NAME_MAX];
  gethostname(hostname, HOST_NAME_MAX);
  PRINT_INFO("Rantanplan v%u.%u %srunning on %s", VERSION_MAJOR, VERSION_MINOR,
             DEBUG_MODE ? "debug build " : "", hostname);
}

Config get_config(const options::Options &options) {
  Config config;
  config.domain_file = options.get<std::string>("domain");
  config.problem_file = options.get<std::string>("problem");

  if (options.is_present<std::string>("planner")) {
    config.planner = options.get<std::string>("planner");
  }

  if (options.is_present<std::string>("plan-output")) {
    config.plan_output_file = options.get<std::string>("plan-output");
  }

  if (options.is_present<int>("log-level")) {
    auto log_level = options.get<int>("log-level");
    if (0 <= log_level && log_level <= 3) {
      config.log_level =
          static_cast<logging::Level>(options.get<int>("log-level"));
    } else {
      PRINT_WARN("Invalid log level, using level \'%s\' instead",
                 logging::level_name(config.log_level));
    }
  }

  config.log_parser = options.get<bool>("log-parser");
  config.log_encoding = options.get<bool>("log-encoding");

  return config;
}

int main(int argc, char *argv[]) {
  options::Options options(argv[0]);
  options.add_positional_option<std::string>("domain", "The pddl domain file");
  options.add_positional_option<std::string>("problem",
                                             "The pddl problem file");
  options.add_option<std::string>("planner", 'x', "Planner to use", "foreach");
  options.add_option<std::string>("plan-output", 'o',
                                  "File to output the plan to", "plan.txt");
  options.add_option<int>(
      "log-level", 'v',
      "Control the log level (0: Error, 1: Warn, 2: Info, 3: Debug)", "2");
  options.add_option<bool>("log-parser", 'p', "Enable logging for the parser",
                           "1");
  options.add_option<bool>("log-encoding", 'e',
                           "Enable logging for the encoder", "1");
  options.add_option<bool>("help", 'h', "Display usage information");

  try {
    options.parse(argc, argv);
  } catch (const options::OptionException &e) {
    PRINT_ERROR(e.what());
    PRINT_INFO("Try --help for further information");
    return 1;
  }

  if (options.is_present<bool>("help")) {
    options.print_usage();
    return 0;
  }

  auto config = get_config(options);

  logging::default_appender.set_level(config.log_level);

  if (config.log_parser) {
    parser::logger.add_appender(logging::default_appender);
  }
  if (config.log_encoding) {
    encoding::logger.add_appender(logging::default_appender);
  }

  print_version();

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

  lexer::Lexer<parser::Rules> lexer;
  parser::ast::AST ast;
  try {
    PRINT_INFO("Parsing problem...");
    auto domain_tokens =
        lexer.lex(config.domain_file, std::istreambuf_iterator<char>(domain),
                  std::istreambuf_iterator<char>());
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
    std::unique_ptr<encoding::Encoder> encoder;
    if (config.planner == "sequential") {
      PRINT_INFO("Using sequential encoding");
      encoder = std::make_unique<encoding::SequentialEncoder>(problem);
    } else if (config.planner == "foreach") {
      PRINT_INFO("Using foreach encoding");
      encoder = std::make_unique<encoding::ForeachEncoder>(problem);
    } else {
      PRINT_ERROR("Unknown planner type \'%s\'", config.planner.c_str());
      return 1;
    }
    encoder->encode();
    encoder->plan(config);
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
