#ifndef RANTANPLAN_OPTIONS_HPP
#define RANTANPLAN_OPTIONS_HPP

#include "config.hpp"
#include "options/options.hpp"
#include "logging/logging.hpp"

#include <string>

extern logging::Logger main_logger;

inline options::Options set_options(const std::string &name) {
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
  options.add_option<unsigned int>({"num-solvers", 'i'},
                                   "Maximum number solvers");
  options.add_option<unsigned int>(
      {"solver-timeout", 'u'},
      "Time for individual solvers before getting interrupted");
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

inline void set_config(Config &config, const options::Options &options) {
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
    if (o.value < 0.0f || o.value > 1.0f) {
      LOG_WARN(main_logger, "Preprocess progress should be within [0, 1]");
    }
    config.preprocess_progress = std::clamp(o.value, 0.0f, 1.0f);
  }

  if (const auto &o = options.get<std::string>("encoding"); o.count > 0) {
    config.parse_encoding(o.value);
  }

  if (const auto &o = options.get<std::string>("solver"); o.count > 0) {
    config.parse_solver(o.value);
  }

  if (const auto &o = options.get<float>("step-factor"); o.count > 0) {
    if (o.value > 1.0f) {
      LOG_WARN(main_logger, "Step factor should be at least 1.0");
    }
    config.step_factor = std::max(o.value, 1.0f);
  }

  if (const auto &o = options.get<unsigned int>("max-steps"); o.count > 0) {
    config.max_steps = o.value;
  }

  if (const auto &o = options.get<unsigned int>("num-solvers"); o.count > 0) {
    if (o.value < 2) {
      LOG_WARN(main_logger, "Number of solvers should be at least 2");
    }
    config.num_solvers = std::max(o.value, 2u);
  }

  if (const auto &o = options.get<unsigned int>("solver-timeout");
      o.count > 0) {
    config.solver_timeout = std::chrono::seconds{o.value};
  }

  if (const auto &o = options.get<unsigned int>("num-threads"); o.count > 0) {
    if (o.value < 1) {
      LOG_WARN(main_logger, "Number of threads should be at least 1");
    }
    config.num_threads = std::max(o.value, 1u);
  }

  if (const auto &o = options.get<unsigned int>("dnf-threshold"); o.count > 0) {
    config.dnf_threshold = o.value;
  }

  if (options.get<bool>("debug-log").count > 0) {
    config.log_level = logging::Level::DEBUG;
  }
}

#endif /* end of include guard: RANTANPLAN_OPTIONS_HPP */
