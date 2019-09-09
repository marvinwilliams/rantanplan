#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "build_config.hpp"
#include "util/logger.hpp"

struct Config {
  logging::Level log_level =
      debug_mode ? logging::Level::DEBUG : logging::Level::INFO;
  bool log_parser = true;
  bool log_normalize = false;
  bool log_visitor = false;
};

#endif /* end of include guard: CONFIG_HPP */
