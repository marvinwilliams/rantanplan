#include "engine/oneshot_engine.hpp"
#include "engine/engine.hpp"
#include "model/normalized/model.hpp"
#include "planner/planner.hpp"
#include "planner/sat_planner.hpp"
#include "preprocess/preprocess.hpp"
#include "util/timer.hpp"

#include <chrono>

OneshotEngine::OneshotEngine(
    const std::shared_ptr<normalized::Problem> &problem,
    const Config &config) noexcept
    : Engine(problem, config) {}

Engine::Status OneshotEngine::start_impl() noexcept {
  Preprocessor preprocessor{problem_, config_};
  while (preprocessor.get_progress() <= config_.preprocess_progress &&
         preprocessor.refine()) {
  }
  LOG_INFO(engine_logger, "Actions: %lu", preprocessor.get_num_actions());
  LOG_INFO(engine_logger, "Preprocess progress: %.3f",
           preprocessor.get_progress());
  auto problem = preprocessor.extract_problem();
  SatPlanner planner{config_};
  if (config_.timeout == std::chrono::seconds::zero()) {
    planner.find_plan(problem, config_.max_steps, std::chrono::seconds::zero());
  } else {
    auto searchtime =
        std::max(std::chrono::duration_cast<std::chrono::seconds>(
                     config_.timeout - util::global_timer.get_elapsed_time()),
                 std::chrono::seconds{1});
    planner.find_plan(problem, config_.max_steps, searchtime);
  }
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
