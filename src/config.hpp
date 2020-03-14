#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "logging/logging.hpp"
#include "options/options.hpp"
#include "util/timer.hpp"

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
    Ground,
    Fixed,
    Oneshot,
    Interrupt
#ifdef PARALLEL
    ,
    Parallel
#endif
  };
  enum class ParameterSelection {
    MostFrequent,
    MinNew,
    MaxRigid,
    ApproxMinNew,
    ApproxMaxRigid,
    FirstEffect
  };
  enum class CachePolicy { None, NoUnsuccessful, Unsuccessful };
  enum class PruningPolicy { Eager, Ground, Trivial };
  enum class Encoding { Sequential, Foreach, LiftedForeach, Exists };
  enum class Solver { Ipasir };

#ifdef PARALLEL
  std::atomic_bool global_stop_flag = false;
#endif

  // General
  std::string domain_file = "";
  std::string problem_file = "";
  PlanningMode planning_mode = PlanningMode::Oneshot;
  util::Seconds timeout = util::inf_time;
  std::optional<std::string> plan_file = std::nullopt;

  // Grounding
  ParameterSelection parameter_selection = ParameterSelection::ApproxMinNew;
  CachePolicy cache_policy = CachePolicy::Unsuccessful;
  PruningPolicy pruning_policy = PruningPolicy::Ground;
  float target_groundness = 1.0f;
  unsigned int granularity = 3;
  util::Seconds grounding_timeout = util::inf_time;

  // Encoding
  Encoding encoding = Encoding::Foreach;
  bool parameter_implies_action = false;
  // Number of dnf clauses with more than 1 literal to be converted to cnf.
  // Above this limit, helper variables are introduced to mitigate a too high
  // clause count.
  unsigned int dnf_threshold = 4;

  // Planning
  Solver solver = Solver::Ipasir;
  float step_factor = 1.4f;
  unsigned int max_skip_steps = 3;
  util::Seconds step_timeout = util::Seconds{10};
  util::Seconds solver_timeout = util::Seconds{60};

#ifdef PARALLEL
  // Parallel
  unsigned int num_threads = 2;
#endif

  // Logging
  logging::Level log_level = logging::Level::INFO;

  void parse_planning_mode(const std::string &input) {
    if (input == "parse") {
      planning_mode = PlanningMode::Parse;
    } else if (input == "normalize") {
      planning_mode = PlanningMode::Normalize;
    } else if (input == "ground") {
      planning_mode = PlanningMode::Ground;
    } else if (input == "fixed") {
      planning_mode = PlanningMode::Fixed;
    } else if (input == "oneshot") {
      planning_mode = PlanningMode::Oneshot;
    } else if (input == "interrupt") {
      planning_mode = PlanningMode::Interrupt;
#ifdef PARALLEL
    } else if (input == "parallel") {
      planning_mode = PlanningMode::Parallel;
#endif
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
    } else if (input == "lf" || input == "liftedforeach") {
      encoding = Encoding::LiftedForeach;
    } else if (input == "e" || input == "exists") {
      encoding = Encoding::Exists;
    } else {
      throw ConfigException{"Unknown encoding \'" + std::string{input} + "\'"};
    }
  }

  void parse_parameter_selection(const std::string &input) {
    if (input == "mostfrequent") {
      parameter_selection = ParameterSelection::MostFrequent;
    } else if (input == "minnew") {
      parameter_selection = ParameterSelection::MinNew;
    } else if (input == "maxrigid") {
      parameter_selection = ParameterSelection::MaxRigid;
    } else if (input == "approxminnew") {
      parameter_selection = ParameterSelection::ApproxMinNew;
    } else if (input == "approxmaxrigid") {
      parameter_selection = ParameterSelection::ApproxMaxRigid;
    } else if (input == "firsteffect") {
      parameter_selection = ParameterSelection::FirstEffect;
    } else {
      throw ConfigException{"Unknown parameter selection \'" +
                            std::string{input} + "\'"};
    }
  }

  void parse_cache_policy(const std::string &input) {
    if (input == "none") {
      cache_policy = CachePolicy::None;
    } else if (input == "nounsuccessful") {
      cache_policy = CachePolicy::NoUnsuccessful;
    } else if (input == "unsuccessful") {
      cache_policy = CachePolicy::Unsuccessful;
    } else {
      throw ConfigException{"Unknown cache policy \'" + std::string{input} +
                            "\'"};
    }
  }

  void parse_pruning_policy(const std::string &input) {
    if (input == "eager") {
      pruning_policy = PruningPolicy::Eager;
    } else if (input == "ground") {
      pruning_policy = PruningPolicy::Ground;
    } else if (input == "trivial") {
      pruning_policy = PruningPolicy::Trivial;
    } else {
      throw ConfigException{"Unknown pruning policy \'" + std::string{input} +
                            "\'"};
    }
  }
};

#endif /* end of include guard: CONFIG_HPP */
