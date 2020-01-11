#include "build_config.hpp"
#include "config.hpp"
#include "engine/engine.hpp"
#include "lexer/lexer.hpp"
#include "logging/logging.hpp"
#include "model/normalize.hpp"
#include "model/to_string.hpp"
#include "options/options.hpp"
#include "pddl/ast/ast.hpp"
#include "pddl/model_builder.hpp"
#include "pddl/parser.hpp"
#include "planner/planner.hpp"

#include <climits>
#include <memory>
#include <string>
#include <unistd.h>

logging::Logger main_logger{"Main"};
logging::Logger parser_logger{"Parser"};
logging::Logger normalize_logger{"Normalize"};
logging::Logger engine_logger{"Engine"};
logging::Logger planner_logger{"Planner"};
logging::Logger encoding_logger{"Encoding"};

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
  options.add_option<std::string>({"planning-mode", 'm'}, "Planning mode");
  options.add_option<unsigned int>({"timeout", 't'}, "Timeout");
  options.add_option<std::string>({"plan-file", 'o'},
                                  "File to output the plan to");
  options.add_option<std::string>({"preprocess-mode", 'c'},
                                  "Select preprocess mode");
  options.add_option<bool>({"parallel-preprocess", 'p'},
                           "Use parallel preprocessing");
  options.add_option<float>({"preprocess-progress", 'r'},
                            "Limit preprocessing progress to this percentage");
  options.add_option<std::string>({"encoding", 'e'}, "Encoding to use");
  options.add_option<std::string>({"solver", 's'},
                                  "Select sat solver interface");
  options.add_option<float>({"step-factor", 'f'}, "Step factor");
  options.add_option<unsigned int>({"max-steps", 'l'},
                                   "Maximum number of steps");
  options.add_option<unsigned int>({"num-threads", 'j'}, "Number of threads");
  options.add_option<unsigned int>({"dnf-threshold", 'd'}, "DNF threshold");
  options.add_option<bool>({"debug-log", 'v'}, "Enable debug logging");
  options.add_option<bool>({"log-parser", 'R'},
                           "Enable debug output for the parser");
  options.add_option<bool>({"log-preprocess", 'P'},
                           "Enable debug output for the preprocessing");
  options.add_option<bool>({"log-normalize", 'N'},
                           "Enable debug output for normalizing");
  options.add_option<bool>({"log-encoding", 'E'},
                           "Enable debug output for the encoder");
  options.add_option<bool>({"help", 'h'}, "Display usage information");
  return options;
}

void set_config(Config &config, const options::Options &options) {
  if (const auto &domain = options.get<std::string>("domain");
      domain.count > 0) {
    config.domain_file = domain.value;
  } else {
    throw ConfigException{"Domain file required"};
  }

  if (const auto &problem = options.get<std::string>("problem");
      problem.count > 0) {
    config.problem_file = problem.value;
  } else {
    throw ConfigException{"Problem file required"};
  }

  if (const auto &o = options.get<std::string>("planning-mode"); o.count > 0) {
    config.parse_planning_mode(o.value);
  }

  if (const auto &o = options.get<unsigned int>("timeout"); o.count > 0) {
    config.timeout = std::chrono::seconds{o.value};
  }

  if (const auto &o = options.get<std::string>("plan-file"); o.count > 0) {
    if (o.value == "") {
      throw ConfigException{"Plan file must not be empty"};
    }
    config.plan_file = o.value;
  }

  if (const auto &o = options.get<std::string>("preprocess-mode");
      o.count > 0) {
    config.parse_preprocess_mode(o.value);
  }

  config.parallel_preprocess =
      options.get<bool>("parallel-preprocess").count > 0;

  if (const auto &o = options.get<float>("preprocess-progress"); o.count > 0) {
    config.preprocess_progress = std::clamp(o.value, 0.0f, 1.0f);
  }

  if (const auto &o = options.get<std::string>("encoding"); o.count > 0) {
    config.parse_encoding(o.value);
  }

  if (const auto &o = options.get<std::string>("solver"); o.count > 0) {
    config.parse_solver(o.value);
  }

  if (const auto &o = options.get<float>("step-factor"); o.count > 0) {
    config.step_factor = std::max(o.value, 1.0f);
  }

  if (const auto &o = options.get<unsigned int>("max-steps"); o.count > 0) {
    config.step_factor = o.value;
  }

  if (const auto &o = options.get<unsigned int>("num-threads"); o.count > 0) {
    config.num_threads = std::max(o.value, 1u);
  }

  if (const auto &o = options.get<unsigned int>("dnf-threshold"); o.count > 0) {
    config.dnf_threshold = o.value;
  }

  if (options.get<bool>("debug-log").count > 0) {
    config.log_level = logging::Level::DEBUG;
  }
}

std::unique_ptr<Engine> get_engine(const Config &config) {}

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
    set_config(config, options);
  } catch (const ConfigException &e) {
    PRINT_ERROR(e.what());
    PRINT_INFO("Try %s --help for further information", argv[0]);
    return 1;
  }

  logging::default_appender.set_level(config.log_level);
  main_logger.add_appender(logging::default_appender);
  parser_logger.add_appender(logging::default_appender);
  normalize_logger.add_appender(logging::default_appender);
  engine_logger.add_appender(logging::default_appender);
  planner_logger.add_appender(logging::default_appender);
  encoding_logger.add_appender(logging::default_appender);

  print_version();

  std::unique_ptr<parsed::Problem> parsed_problem;

  LOG_INFO(main_logger, "Reading problem...");

  pddl::Parser parser;

  try {
    auto ast = parser.parse(config.domain_file, config.problem_file);
    pddl::ModelBuilder builder;
    parsed_problem = builder.parse(ast);
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

  if (config.planning_mode == Config::PlanningMode::Parse) {
    PRINT_INFO("Done");
    return 0;
  }

  LOG_INFO(main_logger, "Normalizing...");
  auto problem = normalize(*parsed_problem);
  if (config.planning_mode == Config::PlanningMode::Normalize) {
    PRINT_INFO("Done");
    return 0;
  }

  std::unique_ptr<Engine> engine;

  switch (config.planning_mode) {
  case Config::PlanningMode::OneShot:
    LOG_INFO(main_logger, "Using oneshot planning");
    return std::make_unique<OneShotEngine>(problem, config);
  case Config::PlanningMode::Interrupt:
    LOG_INFO(main_logger, "Using interrupt planning");
    return std::make_unique<InterruptEngine>(problem, config);
  case Config::PlanningMode::Parallel:
    LOG_INFO(main_logger, "Using parallel planning");
    return std::make_unique<ParallelEngine>(problem, config);
  default:
    assert(false);
    return std::make_unique<OneShotEngine>(problem, config);
  }

  LOG_INFO(main_logger, "Starting search engine");

  engine->start();

  if (engine->get_status() == Engine::Status::Success) {
    std::cout << to_string(engine->get_plan(), problem) << std::endl;
    if (config.plan_file) {
      std::ofstream s{*config.plan_file};
      s << to_string(engine->get_plan(), problem);
    }
  }

  PRINT_INFO("Done");

  return 0;
}
