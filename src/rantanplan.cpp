#include "build_config.hpp"
#include "config.hpp"
/* #include "encoding/encoding.hpp" */
#include "encoding/foreach.hpp"
/* #include "encoding/sequential_1.hpp" */
/* #include "encoding/sequential_2.hpp" */
/* #include "encoding/sequential_3.hpp" */
#include "lexer/lexer.hpp"
#include "logging/logging.hpp"
#include "model/normalize.hpp"
#include "model/preprocess.hpp"
#include "model/support.hpp"
#include "model/to_string.hpp"
#include "options/options.hpp"
#include "pddl/ast.hpp"
#include "pddl/parser.hpp"
#include "pddl/pddl_visitor.hpp"
#include "pddl/tokens.hpp"
#include "planning/planner.hpp"
#include "planning/sat_planner.hpp"

#include <climits>
#include <memory>
#include <string>
#include <unistd.h>

void print_version() noexcept {
  char hostname[HOST_NAME_MAX];
  gethostname(hostname, HOST_NAME_MAX);
  PRINT_INFO("Rantanplan v%u.%u %s%srunning on %s", VERSION_MAJOR,
             VERSION_MINOR, DEBUG_MODE ? "debug build " : "",
             DEBUG_LOG_ACTIVE ? "with debug log " : "", hostname);
}

options::Options set_options(const std::string &name) {
  options::Options options(name);
  options.add_positional_option<std::string>("domain", "The pddl domain file");
  options.add_positional_option<std::string>("problem",
                                             "The pddl problem file");
  options.add_option<std::string>("planner", 'x', "Planner to use", "foreach");
  options.add_option<std::string>("preprocess", 'p',
                                  "Select level of preprocessing", "rigid");
  options.add_option<std::string>("solver", 's', "Select sat solver interface",
                                  "ipasir");
  options.add_option<std::string>("plan-output", 'o',
                                  "File to output the plan to", "plan.txt");
  options.add_option<bool>("logging", 'v', "Enable debug logging", "0");
  options.add_option<bool>("log-parser", 'R', "Enable logging for the parser",
                           "1");
  options.add_option<bool>("log-preprocess", 'P',
                           "Enable logging for the problem preprocessor", "1");
  options.add_option<bool>("log-normalize", 'N',
                           "Enable logging for normalizing", "1");
  options.add_option<bool>("log-support", 'S',
                           "Enable logging for generating support", "1");
  options.add_option<bool>("log-encoding", 'E',
                           "Enable logging for the encoder", "1");
  options.add_option<bool>("help", 'h', "Display usage information");
  return options;
}

void get_config(const options::Options &options, Config &config) {
  config.domain_file = options.get<std::string>("domain");
  config.problem_file = options.get<std::string>("problem");

  if (options.count<std::string>("planner") > 0) {
    config.set_planner_from_string(options.get<std::string>("planner"));
  }

  if (options.count<std::string>("preprocess") > 0) {
    config.set_preprocess_from_string(options.get<std::string>("preprocess"));
  }

  if (options.count<std::string>("solver") > 0) {
    config.set_solver_from_string(options.get<std::string>("solver"));
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
  config.log_preprocess = options.get<bool>("log-preprocess");
  config.log_encoding = options.get<bool>("log-encoding");
}

std::unique_ptr<planning::Planner> get_planner(const Config &config,
                                               const model::Problem &problem) {
  switch (config.planner) {
  case Config::Planner::Sequential1:
    PRINT_INFO("Using sequential encoding 1");
    return std::make_unique<planning::SatPlanner<encoding::ForeachEncoder>>(
        problem);
  case Config::Planner::Sequential2:
    PRINT_INFO("Using sequential encoding 2");
    return std::make_unique<planning::SatPlanner<encoding::ForeachEncoder>>(
        problem);
  case Config::Planner::Sequential3:
    PRINT_INFO("Using sequential encoding 3");
    return std::make_unique<planning::SatPlanner<encoding::ForeachEncoder>>(
        problem);
  case Config::Planner::Foreach:
    PRINT_INFO("Using foreach encoding");
    return std::make_unique<planning::SatPlanner<encoding::ForeachEncoder>>(
        problem);
  case Config::Planner::Preprocess:
    assert(false);
  default:
    return std::unique_ptr<planning::Planner>{};
  }
}

int main(int argc, char *argv[]) {
  auto options = set_options(argv[0]);
  try {
    options.parse(argc, argv);
  } catch (const options::OptionException &e) {
    PRINT_ERROR(e.what());
    PRINT_INFO("Try %s --help for further information", argv[0]);
    return 1;
  }

  if (options.count<bool>("help") > 0) {
    options.print_usage();
    return 0;
  }

  Config config;
  try {
    get_config(options, config);
  } catch (const ConfigException &e) {
    PRINT_ERROR(e.what());
    PRINT_INFO("Try %s --help for further information", argv[0]);
    return 1;
  }

  logging::default_appender.set_level(config.log_level);

  if (config.log_parser) {
    pddl::Parser::logger.add_appender(logging::default_appender);
  }
  if (config.log_normalize) {
    model::normalize_logger.add_appender(logging::default_appender);
  }
  if (config.log_support) {
    support::logger.add_appender(logging::default_appender);
  }
  if (config.log_preprocess) {
    preprocess::logger.add_appender(logging::default_appender);
  }

  print_version();

  std::unique_ptr<model::AbstractProblem> abstract_problem;

  try {
    PRINT_INFO("Parsing problem...");
    pddl::Parser parser;
    auto ast = parser.parse(config.domain_file, config.problem_file);

    pddl::PddlAstParser visitor;
    abstract_problem = visitor.parse(ast);
    PRINT_DEBUG("Abstract problem:\n%s",
                model::to_string(*abstract_problem).c_str());
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
  assert(abstract_problem);

  if (config.planner == Config::Planner::Parse) {
    PRINT_INFO("Parsing successful");
    return 0;
  }

  PRINT_INFO("Normalizing problem...");
  auto problem = model::normalize(*abstract_problem);
  PRINT_DEBUG("Normalized problem:\n%s", model::to_string(problem).c_str());

  if (config.planner == Config::Planner::Preprocess) {
    PRINT_INFO("Preprocessing...");
    auto support = support::Support{problem};
    preprocess::preprocess(problem, support, config);
    PRINT_INFO("Preprocessing successful");
    PRINT_DEBUG("Preprocessed problem:\n%s", model::to_string(problem).c_str());
    return 0;
  }

  auto planner = get_planner(config, problem);
  assert(planner);

  auto plan = planner->plan(config);

  if (plan.empty()) {
    return 1;
  }

  if (config.plan_output_file == "") {
    std::cout << planner->to_string(plan) << std::endl;
  } else {
    std::ofstream s{config.plan_output_file};
    s << planner->to_string(plan);
  }

  return 0;
}
