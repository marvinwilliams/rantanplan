#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "logging/logging.hpp"
#include "options/options.hpp"

#include <chrono>
#include <exception>
#include <string>
#ifdef PARALLEL
#include <atomic>
#endif

class ConfigException : public std::exception {
public:
  explicit ConfigException(std::string message) noexcept
      : message_{std::move(message)} {}

  inline const char *what() const noexcept override { return message_.c_str(); }

private:
  std::string message_;
};

struct Config {
  enum class PlanningMode {
    Parse,
    Normalize,
    Preprocess,
    Oneshot,
    Interrupt,
    Parallel
  };
  enum class PreprocessMode { New, Rigid, Free };
  enum class Encoding { Sequential, Foreach, Exists };
  enum class Solver { Ipasir };

  const util::Timer global_timer;
#ifdef PARALLEL
  std::atomic_bool global_stop_flag = false;
#endif

  // General
  std::string domain_file = "";
  std::string problem_file = "";
  PlanningMode planning_mode = PlanningMode::Oneshot;
  std::chrono::seconds timeout = std::chrono::seconds::zero();
  std::optional<std::string> plan_file;

  // Preprocess
  PreprocessMode preprocess_mode = PreprocessMode::New;
  bool parallel_preprocess = false;
  float preprocess_progress = 1.0f;

  // Planning
  Encoding encoding = Encoding::Foreach;
  Solver solver = Solver::Ipasir;
  float step_factor = 1.4f;
  unsigned int max_steps = 0; // 0: Infinity
  // For interrupt planning
  unsigned int num_solvers = 2;
  std::chrono::seconds solver_timeout = std::chrono::seconds{0};
  std::chrono::seconds preprocess_timeout = std::chrono::seconds{0};

  // Parallel
  unsigned int num_threads = 1;

  // Number of dnf clauses with more than 1 literal to be converted to cnf.
  // Above this limit, helper variables are introduced to mitigate a too high
  // clause count.
  unsigned int dnf_threshold = 16;

  // Logging
  logging::Level log_level = logging::Level::INFO;

  void parse_planning_mode(const std::string &input) {
    if (input == "parse") {
      planning_mode = PlanningMode::Parse;
    } else if (input == "normalize") {
      planning_mode = PlanningMode::Normalize;
    } else if (input == "preprocess") {
      planning_mode = PlanningMode::Preprocess;
    } else if (input == "oneshot") {
      planning_mode = PlanningMode::Oneshot;
    } else if (input == "interrupt") {
      planning_mode = PlanningMode::Interrupt;
    } else if (input == "parallel") {
      planning_mode = PlanningMode::Parallel;
    } else {
      throw ConfigException{"Unknown planning mode \'" + std::string{input} +
                            "\'"};
    }
  }

  void parse_encoding(const std::string &input) {
    if (input == "s" || input == "seq" || input == "sequential") {
      encoding = Encoding::Sequential;
    } else if (input == "f" || input == "foreach") {
      encoding = Encoding::Foreach;
    } else if (input == "e" || input == "exists") {
      encoding = Encoding::Exists;
    } else {
      throw ConfigException{"Unknown encoding \'" + std::string{input} + "\'"};
    }
  }

  void parse_preprocess_mode(const std::string &input) {
    if (input == "new") {
      preprocess_mode = PreprocessMode::New;
    } else if (input == "rigid") {
      preprocess_mode = PreprocessMode::Rigid;
    } else if (input == "free") {
      preprocess_mode = PreprocessMode::Free;
    } else {
      throw ConfigException{"Unknown preprocess mode \'" + std::string{input} +
                            "\'"};
    }
  }

  void parse_solver(const std::string &input) {
    if (input == "ipasir") {
      solver = Solver::Ipasir;
    } else {
      throw ConfigException{"Unknown solver \'" + std::string{input} + "\'"};
    }
  }

  bool check_timeout() const noexcept {
    if (timeout > std::chrono::seconds{0} &&
        std::chrono::ceil<std::chrono::seconds>(
            global_timer.get_elapsed_time()) >= timeout) {
      return true;
    }
#ifdef PARALLEL
    if (global_stop_flag.load(std::memory_order_acquire)) {
      return true;
    }
#endif
    return false;
  }
};

#endif /* end of include guard: CONFIG_HPP */
