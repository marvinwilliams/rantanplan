#include "engine/oneshot_engine.hpp"
#include "engine/engine.hpp"
#include "model/normalized/model.hpp"
#include "planner/planner.hpp"
#include "planner/sat_planner.hpp"
#include "preprocess/preprocess.hpp"
#include "util/timer.hpp"

#include <chrono>

using namespace std::chrono_literals;

OneshotEngine::OneshotEngine(
    const std::shared_ptr<normalized::Problem> &problem,
    const Config &config) noexcept
    : Engine(problem, config) {}

Engine::Status OneshotEngine::start_impl() noexcept {
  LOG_INFO(engine_logger, "Using oneshot engine");

  LOG_INFO(engine_logger, "Preprocessing to %.1f%%...",
           config_.preprocess_progress * 100);
  Preprocessor preprocessor{problem_, config_};
  if (!preprocessor.refine(config_.preprocess_progress,
                           config_.preprocess_timeout)) {
    LOG_ERROR(engine_logger, "Preprocessing timed out");
    return Engine::Status::Timeout;
  }

  LOG_INFO(engine_logger, "Preprocessed to %.1f%% resulting in %lu actions",
           preprocessor.get_progress() * 100, preprocessor.get_num_actions());

  auto problem = preprocessor.extract_problem();

  SatPlanner planner{config_};

  auto searchtime = 0s;
  if (config_.timeout > 0s) {
    searchtime =
        std::max(std::chrono::ceil<std::chrono::seconds>(
                     config_.timeout - util::global_timer.get_elapsed_time()),
                 1s);
  }

  if (searchtime > 0s) {
    LOG_INFO(engine_logger, "Planner started with %lu seconds timeout",
             searchtime.count());
  } else {
    LOG_INFO(engine_logger, "Planner started with no timeout");
  }

  planner.find_plan(problem, config_.max_steps, searchtime);

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
