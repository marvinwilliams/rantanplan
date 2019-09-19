#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "build_config.hpp" // VERSION_MAJOR, VERSION_MINOR and debug_mode
#include "logging/logging.hpp"

struct Config {
  // General
  std::string domain_file;
  std::string problem_file;

  // Planning
  std::string planner = "foreach";
  std::string plan_output_file = "";

  // Logging
  logging::Level log_level =
      DEBUG_MODE ? logging::Level::DEBUG : logging::Level::INFO;
  bool log_parser = false;
  bool log_encoding = false;
};

#endif /* end of include guard: CONFIG_HPP */
