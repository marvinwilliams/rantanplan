#include "build_config.hpp"
#include "config.hpp"
#include "encoding/foreach.hpp"
/* #include "encoding/sequential_1.hpp" */
/* #include "encoding/sequential_2.hpp" */
/* #include "encoding/sequential_3.hpp" */
#include "lexer/lexer.hpp"
#include "logging/logging.hpp"
#include "model/normalize.hpp"
#include "model/normalized_problem.hpp"
#include "model/preprocess.hpp"
#include "model/problem.hpp"
#include "model/support.hpp"
#include "model/to_string.hpp"
#include "options/options.hpp"
#include "pddl/ast.hpp"
#include "pddl/parser.hpp"
#include "pddl/pddl_visitor.hpp"
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
  options.add_option<Config::Planner>(
      {"planner", 'x'}, "Planner to use", [](auto input, auto &planner) {
        if (input == "seq1") {
          planner = Config::Planner::Sequential1;
        } else if (input == "seq2") {
          planner = Config::Planner::Sequential2;
        } else if (input == "seq3") {
          planner = Config::Planner::Sequential3;
        } else if (input == "foreach") {
          planner = Config::Planner::Foreach;
        } else if (input == "parse") {
          planner = Config::Planner::Parse;
        } else if (input == "preprocess") {
          planner = Config::Planner::Preprocess;
        } else {
          throw ConfigException{"Unknown planner \'" + std::string{input} +

                                "\'"};
        }
      });
  options.add_option<Config::PreprocessMode>(
      {"preprocess-mode", 'm'}, "Select preprocess mode",
      [](auto input, auto &mode) {
        if (input == "none") {
          mode = Config::PreprocessMode::None;
        } else if (input == "rigid") {
          mode = Config::PreprocessMode::Rigid;
        } else if (input == "precond") {
          mode = Config::PreprocessMode::Preconditions;
        } else if (input == "full") {
          mode = Config::PreprocessMode::Full;
        } else {
          throw ConfigException{"Unknown preprocessing mode \'" +
                                std::string{input} + "\'"};
        }
      });

  options.add_option<Config::PreprocessPriority>(
      {"preprocess-priority", 'p'}, "Select preprocessing priority",
      [](auto input, auto &priority) {
        if (input == "new") {
          priority = Config::PreprocessPriority::New;
        } else if (input == "pruned") {
          priority = Config::PreprocessPriority::Pruned;
        } else {
          throw ConfigException{"Unknown preprocessing mode \'" +
                                std::string{input} + "\'"};
        }
      });

  options.add_option<Config::Solver>(
      {"solver", 's'}, "Select sat solver interface",
      [](auto input, auto &solver) {
        if (input == "ipasir") {
          solver = Config::Solver::Ipasir;
        } else {
          throw ConfigException{"Unknown sat solver \'" + std::string{input} +
                                "\'"};
        }
      });
  options.add_option<double>({"step-factor", 'f'}, "Step factor");
  options.add_option<std::string>({"plan-file", 'o'},
                                  "File to output the plan to");
  options.add_option<bool>({"logging", 'v'}, "Enable debug logging");
  options.add_option<bool>({"log-parser", 'R'},
                           "Enable logging for the parser");
  options.add_option<bool>({"log-preprocess", 'P'},
                           "Enable logging for the problem preprocessor");
  options.add_option<bool>({"log-normalize", 'N'},
                           "Enable logging for normalizing");
  options.add_option<bool>({"log-support", 'S'},
                           "Enable logging for generating support");
  options.add_option<bool>({"log-encoding", 'E'},
                           "Enable logging for the encoder");
  options.add_option<bool>({"help", 'h'}, "Display usage information");
  return options;
}

void set_config(const options::Options &options, Config &config) {
  const auto &domain = options.get<std::string>("domain");
  if (domain.count > 0) {
    config.domain_file = domain.value;
  } else {
    throw ConfigException{"Domain file required"};
  }

  const auto &problem = options.get<std::string>("problem");
  if (problem.count > 0) {
    config.problem_file = problem.value;
  } else {
    throw ConfigException{"Problem file required"};
  }

  const auto &planner = options.get<Config::Planner>("planner");
  if (planner.count > 0) {
    config.planner = planner.value;
  }

  const auto &preprocess_mode =
      options.get<Config::PreprocessMode>("preprocess-mode");
  if (preprocess_mode.count > 0) {
    config.preprocess_mode = preprocess_mode.value;
  }

  const auto &preprocess_priority =
      options.get<Config::PreprocessPriority>("preprocess-priority");
  if (preprocess_priority.count > 0) {
    config.preprocess_priority = preprocess_priority.value;
  }

  const auto &solver = options.get<Config::Solver>("solver");
  if (solver.count > 0) {
    config.solver = solver.value;
  }

  const auto &factor = options.get<double>("step-factor");
  if (factor.count > 0) {
    config.step_factor = factor.value;
  }

  const auto &plan_file = options.get<std::string>("plan-file");
  if (plan_file.count > 0) {
    config.plan_file = plan_file.value;
  }

  config.log_level = options.get<bool>("logging").count;
  if (config.log_level > 3 || (config.log_level > 1 && !DEBUG_LOG_ACTIVE)) {
    PRINT_WARN("Requested verbosity level too high");
  }
}

std::unique_ptr<planning::Planner> get_planner(const Config &config) {
  switch (config.planner) {
  /* case Config::Planner::Sequential1: */
  /*   PRINT_INFO("Using sequential encoding 1"); */
  /*   return std::make_unique<planning::SatPlanner<encoding::ForeachEncoder>>(
   */

  /*       problem); */
  /* case Config::Planner::Sequential2: */
  /*   PRINT_INFO("Using sequential encoding 2"); */
  /*   return std::make_unique<planning::SatPlanner<encoding::ForeachEncoder>>(
   */

  /*       problem); */
  /* case Config::Planner::Sequential3: */
  /*   PRINT_INFO("Using sequential encoding 3"); */
  /*   return std::make_unique<planning::SatPlanner<encoding::ForeachEncoder>>(
   */

  /*       problem); */
  case Config::Planner::Foreach:
    PRINT_INFO("Using foreach encoding");
    return std::make_unique<planning::SatPlanner<encoding::ForeachEncoder>>();
  default:
    assert(false);
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
  } catch (const ConfigException &e) {
    PRINT_ERROR(e.what());
    PRINT_INFO("Try %s --help for further information", argv[0]);
    return 1;
  }

  if (options.present("help")) {
    options.print_usage();
    return 0;
  }

  Config config;
  try {
    set_config(options, config);
  } catch (const ConfigException &e) {
    PRINT_ERROR(e.what());
    PRINT_INFO("Try %s --help for further information", argv[0]);
    return 1;
  }

  logging::default_appender.set_level(
      config.log_level < 2 ? logging::Level::INFO : logging::Level::DEBUG);

  if (config.log_level > 0) {
    normalize_logger.add_appender(logging::default_appender);
    /* support::logger.add_appender(logging::default_appender); */
    preprocess_logger.add_appender(logging::default_appender);
  }
  if (config.log_level > 2) {
    pddl::Parser::logger.add_appender(logging::default_appender);
  }

  print_version();

  std::unique_ptr<Problem> abstract_problem;

  PRINT_INFO("Reading problem...");
  pddl::Parser parser;
  try {
    auto ast = parser.parse(config.domain_file, config.problem_file);

    pddl::PddlAstParser visitor;
    abstract_problem = visitor.parse(ast);
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

  /* PRINT_DEBUG("Abstract problem:\n%s", */
  /*             model::to_string(*abstract_problem).c_str()); */

  if (config.planner == Config::Planner::Parse) {
    PRINT_INFO("Parsing successful");
    return 0;
  }

  PRINT_INFO("Normalizing problem...");
  auto problem = normalize(*abstract_problem);
  PRINT_DEBUG("Normalized problem:\n%s", to_string(*problem).c_str());

  if (config.preprocess_mode != Config::PreprocessMode::None) {
    PRINT_INFO("Preprocessing...");
    preprocess(*problem, config);
    PRINT_INFO("Preprocessing successful");
    PRINT_DEBUG("Preprocessed problem:\n%s", ::to_string(*problem).c_str());
  }
  if (config.planner == Config::Planner::Preprocess) {
    return 0;
  }

  auto planner = get_planner(config);
  assert(planner);

  auto plan = planner->plan(*problem, config);

  if (plan.empty()) {
    return 1;
  }

  if (config.plan_file == "") {
    std::cout << planner->to_string(plan, *problem) << std::endl;
  } else {
    std::ofstream s{config.plan_file};
    s << planner->to_string(plan, *problem);
  }

  return 0;
}
