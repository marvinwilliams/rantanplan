#include "engine/parallel_engine.hpp"
#include "engine/engine.hpp"
#include "model/normalized/model.hpp"
#include "planner/planner.hpp"
#include "planner/sat_planner.hpp"
#include "preprocess/parallel_preprocess.hpp"

#include <atomic>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

ParallelEngine::ParallelEngine(
    const std::shared_ptr<normalized::Problem> &problem) noexcept
    : Engine(problem) {}

Engine::Status ParallelEngine::start_impl() noexcept {
  assert(config.num_threads > 1);

  LOG_INFO(engine_logger, "Using parallel engine");

  ParallelPreprocessor preprocessor{config.num_threads, problem_};

  float progress = preprocessor.get_progress();
  float step_size =
      config.preprocess_progress / static_cast<float>(config.num_threads - 1);
  float next_progress = 0.0f;

  std::vector<std::thread> threads(config.num_threads);
  std::atomic_bool found_plan = false;

  for (unsigned int planner_id = 0; planner_id < config.num_threads;
       ++planner_id) {
    if (config.check_timeout()) {
      break;
    }
    LOG_INFO(engine_logger, "Targeting %.3f groundness",
             next_progress * 100);
    preprocessor.refine(next_progress, config.preprocess_timeout,
                        config.num_threads - planner_id);

    if (found_plan.load(std::memory_order_acquire)) {
      break;
    }

    assert(preprocessor.get_status() !=
           ParallelPreprocessor::Status::Interrupt);

    progress = preprocessor.get_progress();

    LOG_INFO(engine_logger, "Grounded to %.3f groundness resulting in %lu actions",
             progress * 100, preprocessor.get_num_actions());

    LOG_INFO(engine_logger, "Starting planner %u", planner_id);

    threads[planner_id] = std::thread{
        [planner_id, &found_plan, this](auto problem) {
          SatPlanner planner;
          planner.find_plan(problem, config.max_steps, config.solver_timeout);
          if (planner.get_status() == Planner::Status::Success) {
            if (!found_plan.exchange(true, std::memory_order_acq_rel)) {
              config.global_stop_flag.store(true, std::memory_order_seq_cst);
              LOG_INFO(engine_logger, "Planner %u found a plan", planner_id);
              plan_ = planner.get_plan();
            }
          } else if (planner.get_status() == Planner::Status::Timeout) {
            LOG_INFO(engine_logger, "Planner %u timed out", planner_id);
          }
        },
        preprocessor.extract_problem()};

    while (next_progress <= progress) {
      next_progress += step_size;
    }
  }

  std::for_each(threads.begin(), threads.end(), [](auto &t) {
    if (t.joinable()) {
      t.join();
    }
  });

  if (found_plan.load(std::memory_order_acquire)) {
    return Engine::Status::Success;
  }
  return Engine::Status::Timeout;
}
