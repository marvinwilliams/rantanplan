#ifndef ENCODER_HPP
#define ENCODER_HPP

#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "sat/formula.hpp"
#include "sat/model.hpp"

#include <memory>

extern logging::Logger encoding_logger;

class Encoder {
public:
  struct Variable {
    uint_fast64_t sat_var;
    bool this_step = true;
  };

  using Formula = sat::Formula<Variable>;
  using Literal = typename Formula::Literal;

  static constexpr unsigned int DONTCARE = 0;
  static constexpr unsigned int SAT = 1;
  static constexpr unsigned int UNSAT = 2;

  explicit Encoder(const std::shared_ptr<normalized::Problem> &problem) noexcept
      : problem_{problem} {}

  virtual int to_sat_var(Literal l, unsigned int step) const = 0;
  virtual Plan extract_plan(const sat::Model &model,
                            unsigned int num_steps) const = 0;

  const auto &get_init() const noexcept { return init_; }
  const auto &get_universal_clauses() const noexcept {
    return universal_clauses_;
  }
  const auto &get_transition_clauses() const noexcept {
    return transition_clauses_;
  }
  const auto &get_goal_clauses() const noexcept { return goal_; }

  virtual ~Encoder() = default;

protected:
  Formula init_;
  Formula universal_clauses_;
  Formula transition_clauses_;
  Formula goal_;

  std::shared_ptr<normalized::Problem> problem_;
};

#endif /* end of include guard: ENCODER_HPP */
