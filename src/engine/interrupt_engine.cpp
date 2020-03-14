#include "engine/interrupt_engine.hpp"
#include "engine/engine.hpp"
#include "grounder/grounder.hpp"
#include "model/normalized/model.hpp"
#include "planner/planner.hpp"
#include "planner/sat_planner.hpp"
#include "util/timer.hpp"

#include <chrono>

using namespace std::chrono_literals;

InterruptEngine::InterruptEngine(
    const std::shared_ptr<normalized::Problem> &problem) noexcept
    : Engine(problem) {}

Plan InterruptEngine::start_planning_impl() {
  LOG_INFO(engine_logger, "Using interrupt engine");

  Grounder grounder{problem_};
  SatPlanner planner{};

  float groundness = grounder.get_groundness();

  for (unsigned int planner_id = 0; planner_id < config.granularity;
       ++planner_id) {
    if (global_timer.get_elapsed_time() > config.timeout) {
      throw TimeoutException{};
    }
    auto next_groundness =
        static_cast<float>(planner_id + 1) / static_cast<float>(config.granularity);
    if (groundness >= next_groundness) {
      LOG_INFO(engine_logger, "Skipping planner %u", planner_id);
      continue;
    }

    auto problem = grounder.extract_problem();

    LOG_INFO(engine_logger, "Starting planner %u with %.2f seconds timeout",
             planner_id, config.solver_timeout.count());

    try {
      return planner.find_plan(problem, config.solver_timeout);
    } catch (const TimeoutException &e) {
      LOG_INFO(engine_logger, "Planner %u found no solution", planner_id);
    }

    LOG_INFO(engine_logger, "Targeting %.1f groundness", next_groundness);

    grounder.refine(next_groundness, config.grounding_timeout);

    groundness = grounder.get_groundness();

    LOG_INFO(engine_logger,
             "Grounding to %.1f groundness resulting in %lu actions",
             groundness, grounder.get_num_actions());
  }

  auto problem = grounder.extract_problem();

  LOG_INFO(engine_logger, "Starting planner %u with no timeout",
           config.granularity);

  return planner.find_plan(problem, config.solver_timeout);
}
