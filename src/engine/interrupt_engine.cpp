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
  Preprocessor preprocessor{problem_, config_};
  SatPlanner planner{config_};

  if (config_.num_solvers <= 1) {
    if (config_.num_solvers == 1) {
      while (preprocessor.refine()) {
      }
    }
    LOG_INFO(engine_logger, "Actions: %lu", preprocessor.get_num_actions());
    LOG_INFO(engine_logger, "Preprocess progress: %.3f",
             preprocessor.get_progress());
    auto problem = preprocessor.extract_problem();
    auto searchtime = 0s;
    if (config_.timeout > 0s) {
      searchtime =
          std::max(std::chrono::duration_cast<std::chrono::seconds>(
                       config_.timeout - util::global_timer.get_elapsed_time()),
                   1s);
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

  float progress = preprocessor.get_progress();
  float step_size =
      config_.preprocess_progress / static_cast<float>(config_.num_solvers - 1);
  float next_progress = 0.0f;

  bool keep_refining = true;
  while (progress <= config_.preprocess_progress && keep_refining) {
    while (progress <= next_progress) {
      if (config_.timeout > 0s &&
          util::global_timer.get_elapsed_time() >= config_.timeout) {
        return Status::Timeout;
      }
      progress = preprocessor.get_progress();
      if (!preprocessor.refine()) {
        keep_refining = false;
        break;
      }
    }
    LOG_INFO(engine_logger, "Actions: %lu", preprocessor.get_num_actions());
    LOG_INFO(engine_logger, "Preprocess progress: %.3f", progress);

    auto problem = preprocessor.extract_problem();

    std::chrono::seconds searchtime = 0s;
    if (config_.timeout > 0s) {
      searchtime =
          std::max(std::chrono::duration_cast<std::chrono::seconds>(
                       config_.timeout - util::global_timer.get_elapsed_time()),
                   1s);
    }
    if (progress <= config_.preprocess_progress && keep_refining) {
      if (config_.timeout == 0s) {
        searchtime = config_.solver_timeout;
      } else {
        searchtime = std::min(searchtime, config_.solver_timeout);
      }
    }

    planner.reset();
    planner.find_plan(problem, config_.max_steps, searchtime);

    if (planner.get_status() == Planner::Status::Success) {
      plan_ = planner.get_plan();
      return Engine::Status::Success;
    }
    next_progress += step_size;
  }

  if (planner.get_status() == Planner::Status::Timeout) {
    return Status::Timeout;
  }

  return Status::Error;
}
