#include "config.hpp"
#include "encoding/encoding.hpp"
#include "encoding/foreach.hpp"
#include "encoding/sequential_1.hpp"
#include "encoding/sequential_2.hpp"
#include "encoding/sequential_3.hpp"
#include "lexer/lexer_new.hpp"
#include "logging/logging.hpp"
#include "model/normalize.hpp"
#include "options/options.hpp"
#include "pddl/ast.hpp"
#include "pddl/parser.hpp"
#include "pddl/pddl_visitor.hpp"
#include "pddl/tokens.hpp"

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

  if (options.count<std::string>("planner") > 0) {
    config.planner = options.get<std::string>("planner");
  }

  if (options.count<std::string>("plan-output") > 0) {
    config.plan_output_file = options.get<std::string>("plan-output");
  }

  if (options.count<bool>("logging") > 0) {
    config.log_level = logging::Level::DEBUG;
  }

  config.log_parser = options.get<bool>("log-parser");
  config.log_normalize = options.get<bool>("log-normalize");
  config.log_support = options.get<bool>("log-support");
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
  options.add_option<bool>("logging", 'v', "Enable debug logging", "0");
  options.add_option<bool>("log-parser", 'p', "Enable logging for the parser",
                           "1");
  options.add_option<bool>("log-normalize", 'n',
                           "Enable logging for normalizing", "1");
  options.add_option<bool>("log-support", 's',
                           "Enable logging for generating support", "1");
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

  if (options.count<bool>("help") > 0) {
    options.print_usage();
    return 0;
  }

  auto config = get_config(options);

  logging::default_appender.set_level(config.log_level);

  if (config.log_parser) {
    pddl::logger.add_appender(logging::default_appender);
  }
  if (config.log_normalize) {
    model::normalize_logger.add_appender(logging::default_appender);
  }
  if (config.log_support) {
    model::support::logger.add_appender(logging::default_appender);
  }
  if (config.log_encoding) {
    encoding::logger.add_appender(logging::default_appender);
  }

  print_version();

  try {
    PRINT_INFO("Parsing problem...");
    pddl::Parser parser;
    auto ast = parser.parse(config.domain_file, config.problem_file);
    pddl::PddlAstParser visitor;
    auto abstract_problem = visitor.parse(ast);
    PRINT_DEBUG("Abstract problem:\n%s", to_string(abstract_problem).c_str());
    PRINT_INFO("Normalizing problem...");
    auto problem = model::normalize(abstract_problem);
    PRINT_DEBUG("Normalized problem:\n%s", to_string(problem).c_str());
    std::unique_ptr<encoding::Encoder> encoder;
    if (config.planner == "seq1") {
      PRINT_INFO("Using sequential encoding 1");
      encoder = std::make_unique<encoding::Sequential1Encoder>(problem);
    } else if (config.planner == "seq2") {
      PRINT_INFO("Using sequential encoding 2");
      encoder = std::make_unique<encoding::Sequential2Encoder>(problem);
    } else if (config.planner == "seq3") {
      PRINT_INFO("Using sequential encoding 3");
      encoder = std::make_unique<encoding::Sequential3Encoder>(problem);
    } else if (config.planner == "foreach") {
      PRINT_INFO("Using foreach encoding");
      encoder = std::make_unique<encoding::ForeachEncoder>(problem);
    } else if (config.planner == "parse") {
      PRINT_INFO("Parsing successful");
      return 0;
    } else {
      PRINT_ERROR("Unknown planner type \'%s\'", config.planner.c_str());
      return 1;
    }
    encoder->encode();
    encoder->plan(config);
  } catch (const pddl::ParserException &e) {
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
