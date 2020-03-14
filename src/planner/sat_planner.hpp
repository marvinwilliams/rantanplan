#ifndef SAT_PLANNER_HPP
#define SAT_PLANNER_HPP

#include "config.hpp"
#include "encoder/encoder.hpp"
#include "model/normalized/model.hpp"
#include "planner/planner.hpp"
#include "sat/solver.hpp"
#include "util/timer.hpp"

#include <memory>

extern Config config;

std::unique_ptr<Encoder>
get_encoder(const std::shared_ptr<normalized::Problem> &problem,
            util::Seconds timeout) noexcept;

class SatPlanner final : public Planner {
private:
  Plan find_plan_impl(const std::shared_ptr<normalized::Problem> &problem,
                      util::Seconds timeout) override;

  void add_formula(sat::Solver &solver, const Encoder::Formula &formula,
                   unsigned int step, const Encoder &encoder) const noexcept;
  void assume_goal(sat::Solver &solver, unsigned int step,
                   const Encoder &encoder) const noexcept;
};

#endif /* end of include guard: SAT_PLANNER_HPP */
