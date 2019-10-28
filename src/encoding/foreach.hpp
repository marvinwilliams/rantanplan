#ifndef FOREACH_HPP
#define FOREACH_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "model/support.hpp"
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

namespace encoding {

class ForeachEncoder {

public:
  static constexpr unsigned int DONTCARE = 0;
  static constexpr unsigned int SAT = 1;
  static constexpr unsigned int UNSAT = 2;

  static logging::Logger logger;

  struct ActionVariable {
    model::ActionHandle action_handle;
  };

  struct PredicateVariable {
    model::PredicateHandle predicate_handle;
    model::GroundPredicateHandle ground_predicate_handle;
    bool this_step;
  };

  struct ParameterVariable {
    model::ActionHandle action_handle;
    size_t parameter_index;
    size_t constant_index;
  };

  struct HelperVariable {
    size_t value;
  };

  using Variable = std::variant<ActionVariable, PredicateVariable,
                                ParameterVariable, HelperVariable>;
  using Formula = sat::Formula<Variable>;
  using Literal = Formula::Literal;

  explicit ForeachEncoder(const support::Support &support,
                          const Config &config) noexcept;

  int get_sat_var(Literal literal, unsigned int step) const;
  planning::Plan extract_plan(const sat::Model &model, unsigned int step) const
      noexcept;

  const auto &get_initial_clauses() const noexcept { return initial_state_; }

  const auto &get_universal_clauses() const noexcept {
    return universal_clauses_;
  }

  const auto &get_transition_clauses() const noexcept {
    return transition_clauses_;
  }

  const auto &get_goal_clauses() const noexcept { return goal_; }

private:
  void encode_initial_state() noexcept;
  void encode_actions();
  void parameter_implies_predicate(bool is_negated, bool is_effect);
  void interference(bool is_negated);

  void frame_axioms(bool is_negated, size_t dnf_threshold);
  void assume_goal();
  void init_sat_vars();

  const support::Support &support_;
  size_t num_vars_;
  size_t num_helpers_ = 0;
  std::vector<std::vector<unsigned int>> predicates_;
  std::vector<unsigned int> actions_;
  std::vector<std::vector<std::vector<unsigned int>>> parameters_;
  std::vector<unsigned int> helpers_;
  Formula initial_state_;
  Formula universal_clauses_;
  Formula transition_clauses_;
  Formula goal_;
};

} // namespace encoding

#endif /* end of include guard: FOREACH_HPP */
