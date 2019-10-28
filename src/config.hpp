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
  enum class Planner {
    Sequential1,
    Sequential2,
    Sequential3,
    Foreach,
    Parse,
    Preprocess
  };
  enum class PreprocessMode { None, Rigid, Preconditions, Full };
  enum class PreprocessPriority { New, Pruned };
  enum class Solver { Ipasir };

  // General
  std::string domain_file;
  std::string problem_file;

  // Preprocess
  PreprocessMode preprocess_mode = PreprocessMode::Rigid;
  PreprocessPriority preprocess_priority = PreprocessPriority::New;

  // Planning
  Planner planner = Planner::Foreach;
  Solver solver = Solver::Ipasir;
  std::string plan_output_file = "";
  unsigned int max_steps = 0; // 0: Infinity

  // Number of dnf clauses with more than 1 literal to be converted to cnf.
  // Above this limit, helper variables are introudced
  unsigned int dnf_threshold = 16;

  // Logging
  logging::Level log_level = DEBUG_MODE && DEBUG_LOG_ACTIVE
                                 ? logging::Level::DEBUG
                                 : logging::Level::INFO;
  bool log_parser = false;
  bool log_normalize = false;
  bool log_support = false;
  bool log_preprocess = false;
  bool log_encoding = false;

  void set_planner_from_string(const std::string &input) {
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
      throw ConfigException{"Unknown planner \'" + input + "\'"};
    }
  }

  void set_preprocess_mode_from_string(const std::string &input) {
    if (input == "none") {
      preprocess_mode = Config::PreprocessMode::None;
    } else if (input == "rigid") {
      preprocess_mode = Config::PreprocessMode::Rigid;
    } else if (input == "precond") {
      preprocess_mode = Config::PreprocessMode::Preconditions;
    } else if (input == "full") {
      preprocess_mode = Config::PreprocessMode::Full;
    } else {
      throw ConfigException{"Unknown preprocessing mode \'" + input + "\'"};
    }
  }

  void set_preprocess_priority_from_string(const std::string &input) {
    if (input == "new") {
      preprocess_priority = Config::PreprocessPriority::New;
    } else if (input == "pruned") {
      preprocess_priority = Config::PreprocessPriority::Pruned;
    } else {
      throw ConfigException{"Unknown preprocessing mode \'" + input + "\'"};
    }
  }

  void set_solver_from_string(const std::string &input) {
    if (input == "ipasir") {
      solver = Config::Solver::Ipasir;
    } else {
      throw ConfigException{"Unknown sat solver \'" + input + "\'"};
    }
  }
};

#endif /* end of include guard: CONFIG_HPP */
