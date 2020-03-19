#include "engine/parallel_engine.hpp"
#include "engine/engine.hpp"
#include "grounder/parallel_grounder.hpp"
#include "model/normalized/model.hpp"
#include "planner/planner.hpp"
#include "planner/sat_planner.hpp"
#include "util/timer.hpp"

#include <atomic>
#include <thread>

using namespace std::chrono_literals;

ParallelEngine::ParallelEngine(
    const std::shared_ptr<normalized::Problem> &problem) noexcept
    : Engine(problem) {}

Plan ParallelEngine::start_planning_impl() {
  LOG_INFO(engine_logger, "Using parallel engine");
  assert(config.num_threads > 1);

  ParallelGrounder grounder{config.num_threads, problem_};

  LOG_INFO(engine_logger, "Targeting %.3f groundness", 0.f);
  LOG_INFO(engine_logger,
           "Grounding to %.3f groundness resulting in %lu actions",
           grounder.get_groundness(), grounder.get_num_actions());

  std::vector<std::thread> threads(config.num_threads);
  std::atomic_bool found_plan = false;

  Plan plan;

  for (unsigned int planner_id = 0; planner_id < config.num_threads;
       ++planner_id) {
    if (global_timer.get_elapsed_time() > config.timeout) {
      throw TimeoutException{};
    }
    auto next_groundness = static_cast<float>(planner_id + 1) /
                           static_cast<float>(config.num_threads - 1);
    if (grounder.get_groundness() >= next_groundness) {
      LOG_INFO(engine_logger, "Skipping planner %u", planner_id);
      continue;
    }
    if (found_plan.load(std::memory_order_acquire)) {
      break;
    }

    LOG_INFO(engine_logger, "Starting planner %u", planner_id);

    threads[planner_id] = std::thread{
        [planner_id, &found_plan, &plan](auto problem) {
          SatPlanner planner{};
          try {
            Plan thread_plan = planner.find_plan(problem, util::inf_time);
            if (!found_plan.exchange(true, std::memory_order_acq_rel)) {
              config.global_stop_flag.store(true, std::memory_order_seq_cst);
              LOG_INFO(engine_logger, "Planner %u found a plan", planner_id);
              plan = thread_plan;
            }
          } catch (const TimeoutException &e) {
            LOG_INFO(engine_logger, "Planner %u timed out", planner_id);
          }
        },
        grounder.extract_problem()};
    if (planner_id != config.num_threads - 1) {
      LOG_INFO(engine_logger, "Targeting %.3f groundness", next_groundness);
      grounder.refine(next_groundness, config.grounding_timeout,
                      config.num_threads - planner_id - 1);
    }
  }

  std::for_each(threads.begin(), threads.end(), [](auto &t) {
    if (t.joinable()) {
      t.join();
    }
  });

  if (found_plan.load(std::memory_order_acquire)) {
    return plan;
  }
  throw TimeoutException{};
}
