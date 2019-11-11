#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "options/options.hpp"

#include <exception>
#include <string>

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
  std::string plan_file = "";
  unsigned int max_steps = 0; // 0: Infinity

  // Number of dnf clauses with more than 1 literal to be converted to cnf.
  // Above this limit, helper variables are introudced
  unsigned int dnf_threshold = 16;

  // Logging
  unsigned int log_level = 0;
};

#endif /* end of include guard: CONFIG_HPP */
