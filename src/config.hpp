#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "options/options.hpp"

#include <exception>
#include <string>

class ConfigException : public std::exception {
public:
  explicit ConfigException(std::string message) noexcept
      : message_{std::move(message)} {}

  inline const char *what() const noexcept override {
    return message_.c_str();
  }

private:
  std::string message_;
};

struct Config {
  enum class PlanningMode { Parse, Normalize, Preprocess, Plan };
  enum class Planner { Sequential, Foreach, Exists };
  enum class PreprocessMode { None, Rigid, Preconditions, Full };
  enum class PreprocessPriority { New, Rigid, Free };
  enum class Solver { Ipasir };

  // General
  std::string domain_file;
  std::string problem_file;
  PlanningMode mode = PlanningMode::Plan;

  // Preprocess
  PreprocessMode preprocess_mode = PreprocessMode::Rigid;
  PreprocessPriority preprocess_priority = PreprocessPriority::New;
  float preprocess_progress = 1.0f;

  // Planning
  Planner planner = Planner::Foreach;
  Solver solver = Solver::Ipasir;
  std::string plan_file = "";
  float step_factor = 1.4f;
  unsigned int max_steps = 0; // 0: Infinity

  // Number of dnf clauses with more than 1 literal to be converted to cnf.
  // Above this limit, helper variables are introduced
  unsigned int dnf_threshold = 16;

  // Logging
  unsigned int log_level = 0;
};

#endif /* end of include guard: CONFIG_HPP */
