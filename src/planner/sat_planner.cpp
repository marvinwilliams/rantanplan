#include "planner/sat_planner.hpp"
#include "config.hpp"
#include "encoder/encoder.hpp"
#include "encoder/exists_encoder.hpp"
#include "encoder/foreach_encoder.hpp"
#include "encoder/lifted_foreach_encoder.hpp"
#include "encoder/sequential_encoder.hpp"
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
    return std::make_unique<SequentialEncoder>(problem);
  case Config::Encoding::Foreach:
    return std::make_unique<ForeachEncoder>(problem);
  case Config::Encoding::LiftedForeach:
    return std::make_unique<LiftedForeachEncoder>(problem);
  case Config::Encoding::Exists:
    return std::make_unique<ExistsEncoder>(problem);
  }
  return std::make_unique<ForeachEncoder>(problem);
}

Plan SatPlanner::find_plan_impl(
    const std::shared_ptr<normalized::Problem> &problem,
    util::Seconds timeout) {
  util::Timer timer;

  std::unique_ptr<Encoder> encoder;
  try {
    encoder = get_encoder(problem);
  } catch (const TimeoutException &e) {
    LOG_ERROR(planner_logger, "Encoding timed out");
    throw;
  }

  sat::IpasirSolver solver;
  solver << static_cast<int>(Encoder::SAT) << 0;
  solver << -static_cast<int>(Encoder::UNSAT) << 0;
  add_formula(solver, encoder->get_init(), 0, *encoder);
  add_formula(solver, encoder->get_universal_clauses(), 0, *encoder);

  unsigned int step = 0;
  unsigned int skipped_steps = 0;
  float current_step = 1.0f;
  while (true) {
    solver.next_step();
    if (timer.get_elapsed_time() > timeout) {
      break;
    }
    do {
      add_formula(solver, encoder->get_transition_clauses(), step, *encoder);
      ++step;
      add_formula(solver, encoder->get_universal_clauses(), step, *encoder);
    } while (step < static_cast<unsigned int>(current_step));

    assume_goal(solver, step, *encoder);

    auto solve_timeout = skipped_steps >= config.max_skip_steps
                             ? util::inf_time
                             : config.solver_timeout;

    if (solve_timeout == util::inf_time) {
      LOG_INFO(planner_logger, "Trying to solve step %u", step);
    } else {
      LOG_INFO(planner_logger, "Trying to solve step %u for %.2f seconds", step,
               solve_timeout.count());
    }

    util::Timer step_timer;
    solver.solve(timeout - timer.get_elapsed_time(), solve_timeout);
    LOG_INFO(planner_logger, "Solving step %u took %.2f seconds", step,
             util::Seconds{step_timer.get_elapsed_time()}.count());

    switch (solver.get_status()) {
    case sat::Solver::Status::Solved:
      return encoder->extract_plan(solver.get_model(), step);
    case sat::Solver::Status::Timeout:
      break;
    case sat::Solver::Status::Unsolvable:
      skipped_steps = 0;
      break;
    case sat::Solver::Status::Skip:
      LOG_INFO(planner_logger, "Skipped step %u", step);
      ++skipped_steps;
      break;
    default:
      assert(false);
    }
    current_step *= config.step_factor;
  }
  throw TimeoutException{};
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
