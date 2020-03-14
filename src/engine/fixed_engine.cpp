#include "engine/fixed_engine.hpp"
#include "engine/engine.hpp"
#include "model/normalized/model.hpp"
#include "planner/planner.hpp"
#include "planner/sat_planner.hpp"
#include "grounder/grounder.hpp"
#include "util/timer.hpp"

#include <chrono>

using namespace std::chrono_literals;

FixedEngine::FixedEngine(
    const std::shared_ptr<normalized::Problem> &problem) noexcept
    : Engine(problem) {}

Plan FixedEngine::start_planning_impl() {
  LOG_INFO(engine_logger, "Using fixed engine");

  LOG_INFO(engine_logger, "Grounding to %.1f groundness...",
           config.target_groundness);
  Grounder grounder{problem_};

  try {
    grounder.refine(config.target_groundness,
                           config.grounding_timeout);
  } catch (const TimeoutException& e) {
    LOG_ERROR(engine_logger, "Grounder timed out");
    throw;
  }

  LOG_INFO(engine_logger, "Groundness of %.1f resulting in %lu actions",
           grounder.get_groundness(), grounder.get_num_actions());

  auto problem = grounder.extract_problem();

  SatPlanner planner{};

  LOG_INFO(engine_logger, "Planner started with no timeout");

  return planner.find_plan(problem, util::inf_time);
}
