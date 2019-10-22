#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "build_config.hpp" // VERSION_MAJOR, VERSION_MINOR and debug_mode
#include "logging/logging.hpp"

class ConfigException : public std::exception {
public:
  explicit ConfigException(std::string message) noexcept
      : message_{std::move(message)} {}

  [[nodiscard]] inline const char *what() const noexcept override {
    return message_.c_str();
  }

private:
  std::string message_;
};

struct Config {
  enum class Planner { Sequential1, Sequential2, Sequential3, Foreach, Parse, Preprocess };
  enum class Preprocess { None, Rigid, Preconditions, Full };
  enum class Solver { Ipasir };

  // General
  std::string domain_file;
  std::string problem_file;

  // Planning
  Planner planner = Planner::Foreach;
  Preprocess preprocess = Preprocess::Rigid;
  Solver solver = Solver::Ipasir;
  std::string plan_output_file = "";
  unsigned int max_steps = 0; // 0: Infinity

  // Number of dnf clauses with more than 1 literal to be converted to cnf.
  // Above this limit, helper variables are introudced
  unsigned int dnf_threshold = 16;

  // Logging
  logging::Level log_level =
      DEBUG_MODE && DEBUG_LOG_ACTIVE ? logging::Level::DEBUG : logging::Level::INFO;
  bool log_parser = false;
  bool log_normalize = false;
  bool log_support = false;
  bool log_preprocess = false;
  bool log_encoding = false;

  void set_planner_from_string(const std::string& planner_string) {
    if (planner_string == "seq1") {
      planner = Config::Planner::Sequential1;
    } else if (planner_string == "seq2") {
      planner = Config::Planner::Sequential2;
    } else if (planner_string == "seq3") {
      planner = Config::Planner::Sequential3;
    } else if (planner_string == "foreach") {
      planner = Config::Planner::Foreach;
    } else if (planner_string == "parse") {
      planner = Config::Planner::Parse;
    } else if (planner_string == "preprocess") {
      planner = Config::Planner::Preprocess;
    } else {
      throw ConfigException{"Unknown planner \'" + planner_string + "\'"};
    }
  }

  void set_preprocess_from_string(const std::string& preprocess_string) {
    if (preprocess_string == "none") {
      preprocess = Config::Preprocess::None;
    } else if (preprocess_string == "rigid") {
      preprocess = Config::Preprocess::Rigid;
    } else if (preprocess_string == "precond") {
      preprocess = Config::Preprocess::Preconditions;
    } else if (preprocess_string == "full") {
      preprocess = Config::Preprocess::Full;
    } else {
      throw ConfigException{"Unknown preprocessing mode \'" + preprocess_string
        + "\'"};
    }
  }

  void set_solver_from_string(const std::string& solver_string) {
    if (solver_string == "ipasir") {
      solver = Config::Solver::Ipasir;
    } else {
      throw ConfigException{"Unknown sat solver \'" + solver_string + "\'"};
    }
  }
};

#endif /* end of include guard: CONFIG_HPP */
