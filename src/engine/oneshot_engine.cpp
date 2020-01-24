#include "engine/oneshot_engine.hpp"
#include "engine/engine.hpp"
#include "model/normalized/model.hpp"
#include "planner/planner.hpp"
#include "planner/sat_planner.hpp"
#include "preprocess/preprocess.hpp"

#include <chrono>

using namespace std::chrono_literals;

OneshotEngine::OneshotEngine(
    const std::shared_ptr<normalized::Problem> &problem) noexcept
    : Engine(problem) {}

Engine::Status OneshotEngine::start_impl() noexcept {
  LOG_INFO(engine_logger, "Using oneshot engine");

  LOG_INFO(engine_logger, "Preprocessing to %.1f%%...",
           config.preprocess_progress * 100);
  Preprocessor preprocessor{problem_};
  if (!preprocessor.refine(config.preprocess_progress,
                           config.preprocess_timeout)) {
    LOG_ERROR(engine_logger, "Preprocessing timed out");
    return Engine::Status::Timeout;
  }

  LOG_INFO(engine_logger, "Preprocessed to %.1f%% resulting in %lu actions",
           preprocessor.get_progress() * 100, preprocessor.get_num_actions());

  auto problem = preprocessor.extract_problem();

  SatPlanner planner{};

  LOG_INFO(engine_logger, "Planner started with no timeout");

  planner.find_plan(problem, config.max_steps, 0s);

  switch (planner.get_status()) {
  case Planner::Status::Success:
    plan_ = planner.get_plan();
    return Engine::Status::Success;
  case Planner::Status::Timeout:
    return Engine::Status::Timeout;
  default:
    return Engine::Status::Error;
  }
}
