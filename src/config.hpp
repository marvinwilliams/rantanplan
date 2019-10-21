#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "build_config.hpp" // VERSION_MAJOR, VERSION_MINOR and debug_mode
#include "logging/logging.hpp"

struct Config {
  // General
  std::string domain_file;
  std::string problem_file;

  // Planning
  bool preprocess = true;
  std::string planner = "foreach";
  std::string solver = "ipasir";
  std::string plan_output_file = "";
  unsigned int max_steps = 0; // 0: Infinity

  // Logging
  logging::Level log_level =
      DEBUG_MODE && DEBUG_LOG_ACTIVE ? logging::Level::DEBUG : logging::Level::INFO;
  bool log_parser = false;
  bool log_normalize = false;
  bool log_support = false;
  bool log_encoding = false;
};

#endif /* end of include guard: CONFIG_HPP */
