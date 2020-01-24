#include "engine/interrupt_engine.hpp"
#include "engine/engine.hpp"
#include "model/normalized/model.hpp"
#include "planner/planner.hpp"
#include "planner/sat_planner.hpp"
#include "preprocess/preprocess.hpp"
#include "util/timer.hpp"

#include <chrono>

using namespace std::chrono_literals;

InterruptEngine::InterruptEngine(
    const std::shared_ptr<normalized::Problem> &problem) noexcept
    : Engine(problem) {}

Engine::Status InterruptEngine::start_impl() noexcept {
  LOG_INFO(engine_logger, "Using interrupt engine");

  assert(config.num_solvers > 1);

  Preprocessor preprocessor{problem_};
  SatPlanner planner{};

  float progress = preprocessor.get_progress();
  float step_size =
      config.preprocess_progress / static_cast<float>(config.num_solvers - 1);
  float next_progress = 0.0f;

  for (unsigned int planner_id = 0; planner_id < config.num_solvers;
       ++planner_id) {
    if (config.check_timeout()) {
      return Status::Timeout;
    }

    LOG_INFO(engine_logger, "Targeting %.1f%% preprocess progress",
             next_progress * 100);

    preprocessor.refine(next_progress, config.preprocess_timeout);

    progress = preprocessor.get_progress();

    LOG_INFO(engine_logger, "Preprocessed to %.1f%% resulting in %lu actions",
             progress * 100, preprocessor.get_num_actions());

    auto problem = preprocessor.extract_problem();

    auto searchtime = 0s;
    if (progress < config.preprocess_progress &&
        planner_id < config.num_solvers) {
      searchtime = config.solver_timeout;
      LOG_INFO(engine_logger, "Starting planner %u with %lu seconds timeout",
               planner_id, searchtime.count());
    } else {
      LOG_INFO(engine_logger, "Starting planner %u", planner_id);
    }

    planner.reset();
    planner.find_plan(problem, config.max_steps, searchtime);

    if (planner.get_status() == Planner::Status::Success) {
      plan_ = planner.get_plan();
      return Status::Success;
    } else if (planner.get_status() == Planner::Status::Timeout ||
               planner.get_status() == Planner::Status::MaxStepsExceeded) {
      LOG_INFO(engine_logger, "Planner %u found no solution", planner_id);
    } else if (planner.get_status() == Planner::Status::Error) {
      return Engine::Status::Error;
    }

    while (next_progress <= progress) {
      next_progress += step_size;
    }
  }

  return Status::Timeout;
}
