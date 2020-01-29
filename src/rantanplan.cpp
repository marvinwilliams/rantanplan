#include "build_config.hpp"
#include "config.hpp"
#include "engine/engine.hpp"
#include "engine/interrupt_engine.hpp"
#include "engine/oneshot_engine.hpp"
#include "lexer/lexer.hpp"
#include "logging/logging.hpp"
#include "model/normalize.hpp"
#include "model/to_string.hpp"
#include "options/options.hpp"
#include "pddl/ast/ast.hpp"
#include "pddl/model_builder.hpp"
#include "pddl/parser.hpp"
#include "planner/planner.hpp"
#ifdef PARALLEL
#include "engine/parallel_engine.hpp"
#include "preprocess/parallel_preprocess.hpp"
#else
#include "preprocess/preprocess.hpp"
#endif
#include "rantanplan_options.hpp"
#include "util/timer.hpp"

#include <chrono>
#include <climits>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>
#ifdef PARALLEL
#include <atomic>
#endif

using namespace std::chrono_literals;

logging::Logger main_logger{"Main"};
logging::Logger parser_logger{"Parser"};
logging::Logger normalize_logger{"Normalize"};
logging::Logger engine_logger{"Engine"};
logging::Logger planner_logger{"Planner"};
logging::Logger preprocess_logger{"Preprocess"};
logging::Logger encoding_logger{"Encoding"};

Config config;

void print_memory_usage() {
  if (auto f = std::ifstream{"/proc/self/status"}; f.good()) {
    for (std::string line; std::getline(f, line);) {
      std::string key, value;
      std::stringstream{line} >> key >> value;
      if (key == "VmPeak:") {
        std::cout << "Memory used: " << value << " kB" << std::endl;
        return;
      }
    }
  }
  std::cout << "Could not read memory usage" << std::endl;
}

void print_version() noexcept {
  char hostname[HOST_NAME_MAX];
  gethostname(hostname, HOST_NAME_MAX);
  PRINT_INFO("Rantanplan v%u.%u %srunning on %s", VERSION_MAJOR, VERSION_MINOR,
             DEBUG_MODE ? "debug build " : "", hostname);
}

int main(int argc, char *argv[]) {
  std::atexit(print_memory_usage);

  main_logger.add_appender(logging::default_appender);

  std::cout << "Command line:";
  for (int i = 0; i < argc; ++i) {
    std::cout << " " << argv[i];
  }
  std::cout << std::endl;

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

  try {
    set_config(options);
  } catch (const ConfigException &e) {
    PRINT_ERROR(e.what());
    PRINT_INFO("Try %s --help for further information", argv[0]);
    return 1;
  }

  if (config.planning_mode == Config::PlanningMode::Interrupt) {
    if (config.timeout == 0s) {
      if (config.solver_timeout == 0s) {
        config.solver_timeout = 120s;
      }
    } else {
      if (config.preprocess_timeout == 0s) {
        config.preprocess_timeout = (config.timeout / config.num_solvers) / 5;
      }
      if (config.solver_timeout == 0s) {
        config.solver_timeout = (4 * config.timeout / config.num_solvers) / 5;
      }
    }
  }

  logging::default_appender.set_level(config.log_level);
  parser_logger.add_appender(logging::default_appender);
  normalize_logger.add_appender(logging::default_appender);
  engine_logger.add_appender(logging::default_appender);
  planner_logger.add_appender(logging::default_appender);
  preprocess_logger.add_appender(logging::default_appender);
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

  LOG_INFO(main_logger,
           "The parsed problem has %lu types, %lu constants, %lu predicates, "
           "%lu actions",
           parsed_problem->get_types().size(),
           parsed_problem->get_constants().size(),
           parsed_problem->get_predicates().size(),
           parsed_problem->get_actions().size());

  if (config.planning_mode == Config::PlanningMode::Parse) {
    LOG_INFO(main_logger, "Finished");
    return 0;
  }

  LOG_INFO(main_logger, "Normalizing...");

  auto problem = normalize(*parsed_problem);

  if (!problem) {
    LOG_INFO(main_logger, "Problem unsolvable");
    LOG_INFO(main_logger, "Finished");
    return 2;
  }

  LOG_DEBUG(main_logger, "Normalized problem:\n%s",
            to_string(*problem).c_str());

  LOG_INFO(main_logger, "Normalizing resulted in %lu actions",
           problem->actions.size());

  if (config.planning_mode == Config::PlanningMode::Normalize) {
    LOG_INFO(main_logger, "Finished");
    return 0;
  }

  if (config.planning_mode == Config::PlanningMode::Preprocess) {
    LOG_INFO(main_logger, "Preprocessing to %.1f%%...",
             config.preprocess_progress * 100);
#ifdef PARALLEL
    ParallelPreprocessor preprocessor{config.num_threads, problem};
    preprocessor.refine(config.preprocess_progress, config.preprocess_timeout,
                        config.num_threads);
    if (preprocessor.get_status() == ParallelPreprocessor::Status::Timeout) {
#else
    Preprocessor preprocessor{problem};
    if (!preprocessor.refine(config.preprocess_progress,
                             config.preprocess_timeout)) {
#endif
      LOG_ERROR(main_logger, "Preprocessing timed out");
      return 1;
    }

    LOG_INFO(main_logger, "Preprocessed to %.1f%% resulting in %lu actions",
             preprocessor.get_progress() * 100, preprocessor.get_num_actions());
    LOG_DEBUG(main_logger, "Preprocessed problem:\n%s",
              to_string(*preprocessor.extract_problem()).c_str());
    LOG_INFO(main_logger, "Finished");
    return 0;
  }

  if (config.encoding == Config::Encoding::Sequential &&
      config.parameter_implies_action) {
    LOG_WARN(main_logger,
             "Parameter cannot imply actions in the sequential encoding.");
  }

  std::unique_ptr<Engine> engine;

  switch (config.planning_mode) {
  case Config::PlanningMode::Oneshot:
    engine = std::make_unique<OneshotEngine>(problem);
    break;
  case Config::PlanningMode::Interrupt:
    if (config.preprocess_mode == Config::PreprocessMode::OneEffect) {
      LOG_ERROR(main_logger, "Using effect preprocess mode can only be used "
                             "with oneshot planning");
      return 3;
    }
    engine = std::make_unique<InterruptEngine>(problem);
    break;
  case Config::PlanningMode::Parallel:
#ifdef PARALLEL
    engine = std::make_unique<ParallelEngine>(problem);
    break;
#else
    LOG_ERROR(main_logger, "Please compile with parallel support");
    return 3;
#endif
  default:
    assert(false);
    engine = std::make_unique<OneshotEngine>(problem);
  }

  LOG_INFO(main_logger, "Starting search...");

  engine->start();

  switch (engine->get_status()) {
  case Engine::Status::Success:
    LOG_INFO(main_logger, "Found plan of length %lu",
             engine->get_plan().sequence.size());
    std::cout << to_string(engine->get_plan()) << std::endl;
    if (config.plan_file) {
      std::ofstream{*config.plan_file} << to_string(engine->get_plan());
    }
    PRINT_INFO("Finished");
    return 0;
  case Engine::Status::Timeout:
    LOG_ERROR(main_logger, "Search timed out");
    PRINT_INFO("Finished");
    return 1;
  default:
    LOG_ERROR(main_logger, "An error occurred during the search");
  }

  PRINT_INFO("Finished");
  return 2;
}
