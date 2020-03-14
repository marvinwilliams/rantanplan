#ifndef RANTANPLAN_OPTIONS_HPP
#define RANTANPLAN_OPTIONS_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "options/options.hpp"
#include "util/timer.hpp"

#include <string>

extern logging::Logger main_logger;

inline options::Options set_options(const std::string &name) {
  options::Options options(name);

  // General
  options.add_option<bool>({"help", 'h'}, "Display usage information");
  options.add_positional_option<std::string>("domain", "The pddl domain file");
  options.add_positional_option<std::string>("problem",
                                             "The pddl problem file");
  options.add_option<std::string>({"planning-mode", 'm'}, "Planning mode");
  options.add_option<float>({"timeout", 't'},
                            "Global planner timeout in seconds");
  options.add_option<std::string>({"plan-file", 'o'},
                                  "File to output the plan to");

  // Grounding
  options.add_option<std::string>({"parameter-selection", 's'},
                                  "Select preprocess mode");
  options.add_option<std::string>({"cache-policy", 'c'},
                                  "Select cache priority");
  options.add_option<std::string>({"pruning-policy", 'l'},
                                  "Select pruning priority");
  options.add_option<float>({"target-groundness", 'r'},
                            "Specify target groundness");
  options.add_option<unsigned int>({"granularity", 'g'}, "Specify granularity");
  options.add_option<float>({"grounding-timeout", 'w'},
                            "Time for grounding before timing out");

  // Encoding
  options.add_option<std::string>({"encoding", 'e'}, "Encoding to use");
  options.add_option<bool>({"imply-action", 'y'}, "Parameters imply actions");
  options.add_option<unsigned int>({"dnf-threshold", 'd'}, "DNF threshold");

  // Planning
  options.add_option<float>({"step-factor", 'f'}, "Step factor");
  options.add_option<unsigned int>(
      {"max-skip-steps", 'k'}, "Maximum number of steps to consecutively skip");
  options.add_option<float>({"step-timeout", 'u'},
                                   "Time for each step before skipping");
  options.add_option<float>({"solver-timeout", 'z'},
                                   "Time for solvers before being aborted");

#ifdef PARALLEL
  // Parallel
  options.add_option<unsigned int>({"num-threads", 'j'}, "Number of threads");
#endif

  // Logging
  options.add_option<bool>({"debug-log", 'v'}, "Enable debug logging");

  return options;
}

inline void set_config(const options::Options &options, Config &config) {
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

  if (const auto &o = options.get<float>("timeout"); o.count > 0) {
    if (o.value == 0) {
      config.timeout = util::inf_time;
    } else {
      config.timeout = util::Seconds{o.value};
    }
  }

  if (const auto &o = options.get<std::string>("plan-file"); o.count > 0) {
    config.plan_file = o.value;
  }

  if (const auto &o = options.get<std::string>("parameter-selection");
      o.count > 0) {
    config.parse_parameter_selection(o.value);
  }

  if (const auto &o = options.get<std::string>("cache-policy");
      o.count > 0) {
    config.parse_cache_policy(o.value);
  }

  if (const auto &o = options.get<std::string>("pruning-policy");
      o.count > 0) {
    config.parse_pruning_policy(o.value);
  }

  if (const auto &o = options.get<float>("target-groundness"); o.count > 0) {
    if (o.value < 0.0f || o.value > 1.0f) {
      LOG_WARN(
          main_logger,
          "Target groundness should be within [0, 1]. Value will be clamped");
    }
    config.target_groundness = std::clamp(o.value, 0.0f, 1.0f);
  }

  if (const auto &o = options.get<unsigned int>("granularity"); o.count > 0) {
    config.granularity = o.value;
  }

  if (const auto &o = options.get<float>("grounding-timeout");
      o.count > 0) {
    if (o.value == 0) {
      config.grounding_timeout = util::inf_time;
    } else {
      config.grounding_timeout = util::Seconds{o.value};
    }
  }

  if (const auto &o = options.get<std::string>("encoding"); o.count > 0) {
    config.parse_encoding(o.value);
  }

  config.parameter_implies_action = options.get<bool>("imply-action").count > 0;

  if (const auto &o = options.get<unsigned int>("dnf-threshold"); o.count > 0) {
    config.dnf_threshold = o.value;
  }

  if (const auto &o = options.get<float>("step-factor"); o.count > 0) {
    if (o.value < 1.0f) {
      LOG_WARN(main_logger, "Step factor should be at least 1.0");
    }
    config.step_factor = std::max(o.value, 1.0f);
  }

  if (const auto &o = options.get<unsigned int>("max-skip-steps");
      o.count > 0) {
    config.max_skip_steps = o.value;
  }

  if (const auto &o = options.get<float>("step-timeout"); o.count > 0) {
    if (o.value == 0) {
      config.step_timeout = util::inf_time;
    } else {
      config.step_timeout = util::Seconds{o.value};
    }
  }

  if (const auto &o = options.get<float>("solver-timeout");
      o.count > 0) {
    if (o.value == 0) {
      config.solver_timeout = util::inf_time;
    } else {
      config.solver_timeout = util::Seconds{o.value};
    }
  }

#ifdef PARALLEL
  if (const auto &o = options.get<unsigned int>("num-threads"); o.count > 0) {
    if (o.value < 1) {
      LOG_WARN(main_logger, "Number of threads should be at least 1");
    }
    config.num_threads = std::max(o.value, 1u);
  }
#endif

  if (options.get<bool>("debug-log").count > 0) {
    config.log_level = logging::Level::DEBUG;
  }
}

#endif /* end of include guard: RANTANPLAN_OPTIONS_HPP */
