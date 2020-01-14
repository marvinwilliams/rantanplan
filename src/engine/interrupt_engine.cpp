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
    const std::shared_ptr<normalized::Problem> &problem,
    const Config &config) noexcept
    : Engine(problem, config) {}

Engine::Status InterruptEngine::start_impl() noexcept {
  LOG_INFO(engine_logger, "Using interrupt engine");

  assert(config_.num_solvers > 1);

  Preprocessor preprocessor{problem_, config_};
  SatPlanner planner{config_};

  float progress = preprocessor.get_progress();
  float step_size =
      config_.preprocess_progress / static_cast<float>(config_.num_solvers - 1);
  float next_progress = 0.0f;

  unsigned int num_solves = 0;

  while (progress < config_.preprocess_progress || progress == 1.0f) {
    if (config_.timeout > 0s &&
        std::chrono::ceil<std::chrono::seconds>(
            util::global_timer.get_elapsed_time()) >= config_.timeout) {
      LOG_INFO(engine_logger, "Engine started %u solves", num_solves);
      return Status::Timeout;
    }

    if (progress < next_progress || progress == 1.0f) {
      if (!preprocessor.refine()) {
        break;
      }
      progress = preprocessor.get_progress();
      continue;
    }

    LOG_INFO(engine_logger, "Preprocessed to %.1f%% resulting in %lu actions",
             preprocessor.get_progress() * 100, preprocessor.get_num_actions());

    auto problem = preprocessor.extract_problem();

    auto searchtime = config_.solver_timeout;
    if (config_.timeout > 0s) {
      auto remaining =
          std::max(std::chrono::ceil<std::chrono::seconds>(
                       config_.timeout - util::global_timer.get_elapsed_time()),
                   1s);
      searchtime = std::min(searchtime, remaining);
    }

    LOG_INFO(engine_logger, "Planner started with %lu seconds timeout",
             searchtime.count());

    planner.reset();
    planner.find_plan(problem, config_.max_steps, searchtime);

    ++num_solves;

    if (planner.get_status() == Planner::Status::Success) {
      LOG_INFO(engine_logger, "Engine started %u solves", num_solves);
      plan_ = planner.get_plan();
      return Engine::Status::Success;
    }

    while (progress >= next_progress && progress < 1.0f) {
      next_progress += step_size;
    }

    LOG_INFO(engine_logger, "Targeting %.1f%% preprocess progress",
             next_progress * 100);
  }

  LOG_INFO(engine_logger, "Preprocessed to %.1f% resulting in %lu actions",
           preprocessor.get_progress() * 100, preprocessor.get_num_actions());

  auto problem = preprocessor.extract_problem();

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

  planner.reset();
  planner.find_plan(problem, config_.max_steps, searchtime);

  ++num_solves;

  LOG_INFO(engine_logger, "Engine started %u solves", num_solves);

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
