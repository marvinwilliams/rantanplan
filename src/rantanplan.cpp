#include "build_config.hpp"
#include "config.hpp"
/* #include "encoding/exists.hpp" */
#include "encoding/foreach.hpp"
/* #include "encoding/sequential.hpp" */
#include "lexer/lexer.hpp"
#include "logging/logging.hpp"
#include "model/normalize.hpp"
#include "model/normalized/model.hpp"
#include "model/to_string.hpp"
#include "options/options.hpp"
#include "pddl/ast/ast.hpp"
#include "pddl/model_builder.hpp"
#include "pddl/parser.hpp"
#include "planning/planner.hpp"
#include "planning/sat_planner.hpp"
#include "preprocess/preprocess.hpp"

#include <climits>
#include <memory>
#include <string>
#include <unistd.h>

logging::Logger main_logger{"Main"};

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
  options.add_option<Config::PlanningMode>(
      {"mode", 'c'}, "Planning mode", [](auto input, auto &mode) {
        if (input == "parse") {
          mode = Config::PlanningMode::Parse;
        } else if (input == "normalize") {
          mode = Config::PlanningMode::Normalize;
        } else if (input == "preprocess") {
          mode = Config::PlanningMode::Preprocess;
        } else if (input == "plan") {
          mode = Config::PlanningMode::Plan;
        } else {
          throw ConfigException{"Unknown planning mode \'" +
                                std::string{input} + "\'"};
        }
      });
  options.add_option<Config::Planner>(
      {"planner", 'x'}, "Planner to use", [](auto input, auto &planner) {
        if (input == "seq") {
          planner = Config::Planner::Sequential;
        } else if (input == "foreach") {
          planner = Config::Planner::Foreach;
        } else if (input == "exists") {
          planner = Config::Planner::Exists;
        } else {
          throw ConfigException{"Unknown planner \'" + std::string{input} +

                                "\'"};
        }
      });

  options.add_option<Config::PreprocessPriority>(
      {"preprocess-priority", 'p'}, "Select preprocessing priority",
      [](auto input, auto &priority) {
        if (input == "new") {
          priority = Config::PreprocessPriority::New;
        } else if (input == "rigid") {
          priority = Config::PreprocessPriority::Rigid;
        } else if (input == "free") {
          priority = Config::PreprocessPriority::Free;
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
  options.add_option<float>({"step-factor", 'f'}, "Step factor");
  if (PARALLEL_MODE) {
    options.add_option<unsigned int>({"num-threads", 't'}, "Number of threads");
  } else {
    options.add_option<float>({"preprocess-progress", 'i'},
                              "Preprocessing progress");
  }
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

  if (const auto &mode = options.get<Config::PlanningMode>("mode");
      mode.count > 0) {
    config.mode = mode.value;
  }

  if (const auto &planner = options.get<Config::Planner>("planner");
      planner.count > 0) {
    config.planner = planner.value;
  }

  if (const auto &preprocess_priority =
          options.get<Config::PreprocessPriority>("preprocess-priority");
      preprocess_priority.count > 0) {
    config.preprocess_priority = preprocess_priority.value;
  }

  if (const auto &solver = options.get<Config::Solver>("solver");
      solver.count > 0) {
    config.solver = solver.value;
  }

  if (const auto &factor = options.get<float>("step-factor");
      factor.count > 0) {
    config.step_factor = factor.value;
  }

#ifdef PARALLEL
  if (const auto &threads = options.get<unsigned int>("num-threads");
      threads.count > 0) {
    config.num_threads = threads.value;
  }
#else
  if (const auto &iter = options.get<float>("preprocess-progress");
      iter.count > 0) {
    config.preprocess_progress =
        iter.value > 1.0f ? 1.0f : iter.value < 0.0f ? 0.0f : iter.value;
  }
#endif

  if (const auto &plan_file = options.get<std::string>("plan-file");
      plan_file.count > 0) {
    config.plan_file = plan_file.value;
  }

  config.log_level = options.get<bool>("logging").count;
  if (config.log_level > 3 || (config.log_level > 1 && !DEBUG_LOG_ACTIVE)) {
    PRINT_WARN("Requested verbosity level too high");
  }
}

std::unique_ptr<Planner> get_planner(const Config &config) {
  switch (config.planner) {
  case Config::Planner::Sequential:
    LOG_INFO(main_logger, "Using sequential encoding");
    /* return std::make_unique<SatPlanner<SequentialEncoder>>(); */
    return std::make_unique<SatPlanner<ForeachEncoder>>();
  case Config::Planner::Foreach:
    LOG_INFO(main_logger, "Using foreach encoding");
    return std::make_unique<SatPlanner<ForeachEncoder>>();
  case Config::Planner::Exists:
    LOG_INFO(main_logger, "Using exists encoding");
    /* return std::make_unique<SatPlanner<ExistsEncoder>>(); */
    return std::make_unique<SatPlanner<ForeachEncoder>>();
  }
  return std::make_unique<SatPlanner<ForeachEncoder>>();
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
    Support::logger.add_appender(logging::default_appender);
    preprocess_logger.add_appender(logging::default_appender);
    Planner::logger.add_appender(logging::default_appender);
    /* SequentialEncoder::logger.add_appender(logging::default_appender); */
    ForeachEncoder::logger.add_appender(logging::default_appender);
    /* ExistsEncoder::logger.add_appender(logging::default_appender); */
  }
  if (config.log_level > 2) {
    pddl::Parser::logger.add_appender(logging::default_appender);
  }

  print_version();

  std::unique_ptr<parsed::Problem> parsed_problem;

  PRINT_INFO("Reading problem...");
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

  assert(parsed_problem);

  /* PRINT_DEBUG("Abstract problem:\n%s", */
  /*             model::to_string(*abstract_problem).c_str()); */

  if (config.mode == Config::PlanningMode::Parse) {
    PRINT_INFO("Done");
    return 0;
  }

  PRINT_INFO("Normalizing...");
  auto problem = normalize(*parsed_problem);
  if (config.mode == Config::PlanningMode::Normalize) {
    PRINT_INFO("Done");
    return 0;
  }
#ifdef PARALLEL
  PRINT_INFO("Preprocessing...");
  Preprocessor preprocessor{problem};
  auto result = preprocessor.preprocess(*get_planner(config), config);
  problem = std::move(result.first);
  auto plan = std::move(result.second);
#else
  if (config.preprocess_progress > 0.0f) {
    PRINT_INFO("Preprocessing...");
    Preprocessor preprocessor{problem};
    problem = preprocessor.preprocess(config);
  } else {
    PRINT_INFO("Skipping preprocessing");
  }

  if (PARALLEL_MODE) {
    return 0;
  }

  LOG_INFO(main_logger, "Problem has %lu actions", problem.actions.size());
  if (config.mode == Config::PlanningMode::Preprocess) {
    PRINT_INFO("Done");
    return 0;
  }

  auto planner = get_planner(config);

  PRINT_INFO("Planning...");
  auto plan = planner->plan(problem, config);
#endif

  if (plan.empty()) {
    return 1;
  }

  if (config.plan_file == "") {
    std::cout << Planner::to_string(plan, problem) << std::endl;
  } else {
    std::ofstream s{config.plan_file};
    s << Planner::to_string(plan, problem);
  }

  PRINT_INFO("Done");

#ifdef PARALLEL
  preprocessor.kill();
#endif
  return 0;
}
