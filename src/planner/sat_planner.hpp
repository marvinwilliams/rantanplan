#ifndef SAT_PLANNER_HPP
#define SAT_PLANNER_HPP

#include "config.hpp"
#include "encoder/encoder.hpp"
#include "model/normalized/model.hpp"
#include "planner/planner.hpp"
#include "sat/solver.hpp"

#include <memory>

std::unique_ptr<Encoder> get_encoder(const Config &config) noexcept;
std::unique_ptr<sat::Solver> get_solver(const Config &config) noexcept;

class SatPlanner final : public Planner {
public:
  explicit SatPlanner(const Config &config);

private:
  Status find_plan_impl(const normalized::Problem &problem,
                        unsigned int max_steps,
                        std::chrono::seconds timeout) noexcept override;

  void add_formula(sat::Solver &solver, const Encoder::Formula &formula,
                   unsigned int step, const Encoder &encoder) const noexcept;
  void assume_goal(sat::Solver &solver, unsigned int step,
                   const Encoder &encoder) const noexcept;

  const Config &config_;
};

#endif /* end of include guard: SAT_PLANNER_HPP */
