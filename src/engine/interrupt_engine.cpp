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

  unsigned int num_tries = 0;

  Engine::Status result = Engine::Status::Ready;

  while (progress < config_.preprocess_progress &&
         num_tries < config_.num_solvers) {
    LOG_INFO(engine_logger, "Targeting %.1f%% preprocess progress",
             next_progress * 100);

    if (!preprocessor.refine(next_progress, config_.preprocess_timeout)) {
      if (config_.timeout > 0s &&
          std::chrono::ceil<std::chrono::seconds>(
              util::global_timer.get_elapsed_time()) >= config_.timeout) {
        LOG_ERROR(engine_logger, "Preprocessing timed out");
        result = Status::Timeout;
        break;
      }
    }

    progress = preprocessor.get_progress();

    while (next_progress <= progress) {
      next_progress += step_size;
    };

    LOG_INFO(engine_logger, "Preprocessed to %.1f%% resulting in %lu actions",
             progress * 100, preprocessor.get_num_actions());

    auto problem = preprocessor.extract_problem();

    auto searchtime = 0s;
    if (progress < config_.preprocess_progress) {
      searchtime = config_.solver_timeout;
      LOG_INFO(engine_logger, "Starting planner %u with %lu seconds timeout",
               num_tries, searchtime.count());
    } else {
      LOG_INFO(engine_logger, "Starting planner %u", num_tries);
    }

    planner.reset();
    planner.find_plan(problem, config_.max_steps, searchtime);

    ++num_tries;

    if (planner.get_status() == Planner::Status::Success) {
      result = Engine::Status::Success;
      plan_ = planner.get_plan();
      break;
    } else if (planner.get_status() == Planner::Status::Timeout ||
               planner.get_status() == Planner::Status::MaxStepsExceeded) {
      if (config_.timeout > 0s &&
          std::chrono::ceil<std::chrono::seconds>(
              util::global_timer.get_elapsed_time()) >= config_.timeout) {
        LOG_ERROR(engine_logger, "Last planner timed out");
        result = Status::Timeout;
        break;
      }
      LOG_INFO(engine_logger, "Planner timed out");
    } else if (planner.get_status() == Planner::Status::Error) {
      result = Engine::Status::Error;
      break;
    }
  }

  LOG_INFO(engine_logger, "Engine tried %u solves", num_tries);

  return result;
}
