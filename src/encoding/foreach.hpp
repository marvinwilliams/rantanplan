#ifndef FOREACH_HPP
#define FOREACH_HPP

#include "config.hpp"
#include "encoding/support.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "planning/planner.hpp"
#include "sat/formula.hpp"
#include "sat/model.hpp"
#include "util/combination_iterator.hpp"

#include <cstdint>
#include <vector>

class ForeachEncoder {

public:
  static logging::Logger logger;

  struct Variable {
    uint_fast64_t sat_var;
    bool this_step = true;
  };

  using Formula = sat::Formula<Variable>;
  using Literal = typename Formula::Literal;

  static constexpr unsigned int DONTCARE = 0;
  static constexpr unsigned int SAT = 1;
  static constexpr unsigned int UNSAT = 2;

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
  void encode_actions() noexcept;
  void parameter_implies_predicate() noexcept;
  void interference() noexcept;
  void frame_axioms(unsigned int dnf_threshold) noexcept;
  void assume_goal() noexcept;
  void init_sat_vars() noexcept;

  uint_fast64_t num_vars_ = 3;
  std::vector<uint_fast64_t> predicates_;
  std::vector<uint_fast64_t> actions_;
  std::vector<std::vector<std::vector<uint_fast64_t>>> parameters_;
  std::vector<uint_fast64_t> helpers_;
  Formula init_;
  Formula universal_clauses_;
  Formula transition_clauses_;
  Formula goal_;

  normalized::Problem problem_;
  Support support_;
};

#endif /* end of include guard: FOREACH_HPP */
