#ifndef FOREACH_HPP
#define FOREACH_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized_problem.hpp"
#include "model/support.hpp"
#include "model/utils.hpp"
#include "planning/planner.hpp"
#include "sat/formula.hpp"
#include "sat/model.hpp"
#include "util/combinatorics.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

class ForeachEncoder {

public:
  static constexpr unsigned int DONTCARE = 0;
  static constexpr unsigned int SAT = 1;
  static constexpr unsigned int UNSAT = 2;

  static logging::Logger logger;

  struct Variable {
    size_t sat_var;
    bool this_step = true;
  };

  using Formula = sat::Formula<Variable>;
  using Literal = Formula::Literal;

  explicit ForeachEncoder(const normalized::Problem &problem,
                          const Config &config) noexcept;

  int get_sat_var(Literal literal, unsigned int step) const;
  Planner::Plan extract_plan(const sat::Model &model, unsigned int step) const
      noexcept;

  const auto &get_init() const noexcept { return init_; }

  const auto &get_universal_clauses() const noexcept {
    return universal_clauses_;
  }

  const auto &get_transition_clauses() const noexcept {
    return transition_clauses_;
  }

  const auto &get_goal_clauses() const noexcept { return goal_; }

private:
  void encode_init() noexcept;
  void encode_actions();
  void parameter_implies_predicate();
  void interference();

  void frame_axioms(size_t dnf_threshold);
  void assume_goal();
  void init_sat_vars();

  Support support_;
  size_t num_vars_ = 3;
  std::vector<unsigned int> predicates_;
  std::vector<unsigned int> actions_;
  std::vector<std::vector<std::vector<unsigned int>>> parameters_;
  std::vector<unsigned int> helpers_;
  Formula init_;
  Formula universal_clauses_;
  Formula transition_clauses_;
  Formula goal_;
};

#endif /* end of include guard: FOREACH_HPP */
