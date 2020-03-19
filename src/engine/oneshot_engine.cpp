#include "engine/oneshot_engine.hpp"
#include "engine/engine.hpp"
#include "grounder/grounder.hpp"
#include "model/normalized/model.hpp"
#include "planner/planner.hpp"
#include "planner/sat_planner.hpp"
#include "util/timer.hpp"
#include <cstdint>
#include <sys/types.h>

OneshotEngine::OneshotEngine(
    const std::shared_ptr<normalized::Problem> &problem) noexcept
    : Engine(problem) {}

Plan OneshotEngine::start_planning_impl() {
  LOG_INFO(engine_logger, "Using oneshot engine");

  util::Timer timer;
  Grounder grounder{problem_};
  auto smallest_problem = grounder.extract_problem();
  std::unique_ptr<Encoder> smallest_encoder{};
  uint_fast64_t min_encoding_size = std::numeric_limits<uint_fast64_t>::max();
  auto min_grounding = 0.f;

  LOG_INFO(engine_logger, "Targeting %.3f groundness", 0.f);
  LOG_INFO(engine_logger, "Groundness of %.3f resulting in %lu actions",
           grounder.get_groundness(), grounder.get_num_actions());

  try {
    auto encoder = SatPlanner::get_encoder(
        smallest_problem,
        std::min(util::Seconds{10}, util::Seconds{config.grounding_timeout -
                                                  timer.get_elapsed_time()}));
    encoder->encode();
    auto encoding_size = encoder->get_num_vars();
    LOG_INFO(engine_logger, "Encoding as %lu clauses", encoding_size);
    min_encoding_size = encoding_size;
    smallest_encoder = std::move(encoder);
    min_grounding = grounder.get_groundness();
  } catch (const TimeoutException &e) {
  }
  for (size_t i = 1; i <= config.granularity; ++i) {
    auto next_groundness =
        static_cast<float>(i) / static_cast<float>(config.granularity);
    if (grounder.get_groundness() >= next_groundness) {
      continue;
    }

    if (config.grounding_timeout != util::inf_time &&
        timer.get_elapsed_time() > config.grounding_timeout) {
      break;
    }

    LOG_INFO(engine_logger, "Targeting %.3f groundness", next_groundness);

    grounder.refine(next_groundness,
                    config.grounding_timeout - timer.get_elapsed_time());

    if (config.grounding_timeout != util::inf_time &&
        timer.get_elapsed_time() > config.grounding_timeout) {
      break;
    }

    LOG_INFO(engine_logger, "Groundness of %.3f resulting in %lu actions",
             grounder.get_groundness(), grounder.get_num_actions());

    try {
      auto problem = grounder.extract_problem();
      auto encoder = SatPlanner::get_encoder(
          problem,
          std::min(util::Seconds{10}, util::Seconds{config.grounding_timeout -
                                                    timer.get_elapsed_time()}));
      encoder->encode();
      auto encoding_size = encoder->get_num_vars();
      LOG_INFO(engine_logger, "Encoding as %lu clauses", encoding_size);
      if (encoding_size < min_encoding_size) {
        min_encoding_size = encoding_size;
        smallest_encoder = std::move(encoder);
        smallest_problem = std::move(problem);
        min_grounding = grounder.get_groundness();
      }
    } catch (const TimeoutException &e) {
    }
  }

  SatPlanner planner{};
  if (smallest_encoder) {
    LOG_INFO(engine_logger,
             "Smallest encoding with size %lu by problem with %.3f groundness",
             min_encoding_size, min_grounding);

    planner.set_encoder(std::move(smallest_encoder));
  } else if (grounder.get_groundness() == 1.0f) {
    smallest_problem = grounder.extract_problem();
  }
  LOG_INFO(engine_logger, "Planner started with no timeout");
  return planner.find_plan(smallest_problem, util::inf_time);
}
