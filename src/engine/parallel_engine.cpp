#include "engine/parallel_engine.hpp"
#include "engine/engine.hpp"
#include "model/normalized/model.hpp"
#include "planner/planner.hpp"
#include "planner/sat_planner.hpp"
#include "preprocess/parallel_preprocess.hpp"
#include "util/timer.hpp"

#include <atomic>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

ParallelEngine::ParallelEngine(
    const std::shared_ptr<normalized::Problem> &problem,
    const Config &config) noexcept
    : Engine(problem, config) {}

Engine::Status ParallelEngine::start_impl() noexcept {
  LOG_INFO(engine_logger, "Using parallel engine");

  assert(config_.num_threads > 0);

  ParallelPreprocessor preprocessor{config_.num_threads, problem_, config_};

  float progress = preprocessor.get_progress();
  float step_size =
      config_.preprocess_progress / static_cast<float>(config_.num_threads - 1);
  float next_progress = 0.0f;

  unsigned int num_tries = 0;

  std::atomic_bool plan_found = false;
  Engine::Status result = Engine::Status::Ready;

  std::vector<std::thread> threads(config_.num_threads);

  while (progress < config_.preprocess_progress &&
         num_tries < config_.num_threads && !plan_found) {
    LOG_INFO(engine_logger, "Targeting %.1f%% preprocess progress",
             next_progress * 100);

    if (!preprocessor.refine(next_progress, config_.preprocess_timeout,
                             config_.num_threads - num_tries, plan_found)) {
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

    LOG_INFO(engine_logger, "Starting planner %u", num_tries);

    threads[num_tries] = std::thread{
        [&](auto problem, auto id) {
          SatPlanner planner{config_};
          planner.find_plan(problem, config_.max_steps, 0s);
          if (planner.get_status() == Planner::Status::Success) {
            if (!plan_found.exchange(true)) {
              LOG_INFO(engine_logger, "Planner %u found a plan", id);
              plan_ = planner.get_plan();
            }
          }
        },
        preprocessor.extract_problem(), num_tries};

    ++num_tries;
  }

  std::for_each(threads.begin(), threads.end(), [](auto &t) {
    if (t.joinable()) {
      t.join();
    }
  });

  LOG_INFO(engine_logger, "Engine started %u planners", num_tries);

  if (plan_found) {
    return Engine::Status::Success;
  }
  return result;
}
