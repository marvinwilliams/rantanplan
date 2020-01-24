#include "planner/sat_planner.hpp"
#include "config.hpp"
#include "encoder/encoder.hpp"
/* #include "encoder/exists_encoder.hpp" */
#include "encoder/foreach_encoder.hpp"
/* #include "encoder/sequential_encoder.hpp" */
#include "model/normalized/model.hpp"
#include "sat/ipasir_solver.hpp"
#include "sat/solver.hpp"
#include "util/timer.hpp"

#include <chrono>
#include <memory>

using namespace std::chrono_literals;

std::unique_ptr<Encoder>
get_encoder(const std::shared_ptr<normalized::Problem> &problem) noexcept {
  switch (config.encoding) {
  case Config::Encoding::Sequential:
    /* return std::make_unique<SequentialEncoder>(problem, config); */
    return std::make_unique<ForeachEncoder>(problem);
  case Config::Encoding::Foreach:
    return std::make_unique<ForeachEncoder>(problem);
  case Config::Encoding::Exists:
    /* return std::make_unique<ExistsEncoder>(problem, config); */
    return std::make_unique<ForeachEncoder>(problem);
  }
  return std::make_unique<ForeachEncoder>(problem);
}

Planner::Status
SatPlanner::find_plan_impl(const std::shared_ptr<normalized::Problem> &problem,
                           unsigned int max_steps,
                           std::chrono::seconds timeout) noexcept {
  util::Timer timer;
  auto check_timeout = [&]() {
    if (config.check_timeout() ||
        (timeout > 0s && std::chrono::ceil<std::chrono::seconds>(
                             timer.get_elapsed_time()) >= timeout)) {
      return true;
    }
    return false;
  };

  auto encoder = get_encoder(problem);
  if (!encoder->is_initialized()) {
    return Status::Timeout;
  }
  sat::IpasirSolver solver;
  solver << static_cast<int>(Encoder::SAT) << 0;
  solver << -static_cast<int>(Encoder::UNSAT) << 0;
  add_formula(solver, encoder->get_init(), 0, *encoder);
  add_formula(solver, encoder->get_universal_clauses(), 0, *encoder);

  unsigned int step = 0;
  float current_step = 1.0f;
  while (true) {
    solver.next_step();
    if (max_steps > 0 && step >= max_steps) {
      return Status::MaxStepsExceeded;
    }
    if (check_timeout()) {
      return Status::Timeout;
    }
    do {
      add_formula(solver, encoder->get_transition_clauses(), step, *encoder);
      ++step;
      add_formula(solver, encoder->get_universal_clauses(), step, *encoder);
    } while (step < static_cast<unsigned int>(current_step));

    assume_goal(solver, step, *encoder);

    auto searchtime = 0s;
    if (timeout > 0s) {
      searchtime = std::max(std::chrono::ceil<std::chrono::seconds>(
                                timeout - timer.get_elapsed_time()),
                            1s);
    }

    if (searchtime == 0s) {
      LOG_INFO(planner_logger, "Trying to solve step %u", step);
    } else {
      LOG_INFO(planner_logger, "Trying %lu seconsd to solve step %u",
               searchtime.count(), step);
    }

    solver.solve(searchtime);

    switch (solver.get_status()) {
    case sat::Solver::Status::Solved:
      plan_ = encoder->extract_plan(solver.get_model(), step);
      return Status::Success;
    case sat::Solver::Status::Timeout:
      return Status::Timeout;
    case sat::Solver::Status::Error:
      return Status::Error;
    case sat::Solver::Status::Unsolvable:
      break;
    default:
      assert(false);
    }
    current_step *= config.step_factor;
  }
  assert(false);
  return Status::Error;
}

void SatPlanner::add_formula(sat::Solver &solver,
                             const Encoder::Formula &formula, unsigned int step,
                             const Encoder &encoder) const noexcept {
  for (const auto &clause : formula.clauses) {
    for (const auto &literal : clause.literals) {
      solver << encoder.to_sat_var(literal, step);
    }
    solver << 0;
  }
}

void SatPlanner::assume_goal(sat::Solver &solver, unsigned int step,
                             const Encoder &encoder) const noexcept {
  for (const auto &clause : encoder.get_goal_clauses().clauses) {
    for (const auto &literal : clause.literals) {
      solver.assume(encoder.to_sat_var(literal, step));
    }
  }
}
