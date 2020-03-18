#include "engine/oneshot_engine.hpp"
#include "engine/engine.hpp"
#include "grounder/grounder.hpp"
#include "model/normalized/model.hpp"
#include "planner/planner.hpp"
#include "planner/sat_planner.hpp"
#include "util/timer.hpp"
#include <cstdint>

OneshotEngine::OneshotEngine(
    const std::shared_ptr<normalized::Problem> &problem) noexcept
    : Engine(problem) {}

Plan OneshotEngine::start_planning_impl() {
  LOG_INFO(engine_logger, "Using oneshot engine");

  util::Timer timer;
  Grounder grounder{problem_};
  auto smallest_problem = grounder.extract_problem();
  auto smallest_encoder =
      SatPlanner::get_encoder(smallest_problem, config.grounding_timeout);
  uint_fast64_t min_encoding_size =
      smallest_encoder->get_universal_clauses().clauses.size() +
      smallest_encoder->get_transition_clauses().clauses.size();
  auto min_grounding = grounder.get_groundness();
  LOG_INFO(engine_logger, "Groundness of %.3f resulting in %lu actions",
           grounder.get_groundness(), grounder.get_num_actions());
  LOG_INFO(engine_logger, "Encoding as %lu clauses", min_encoding_size);
  for (size_t i = 1; i <= config.granularity; ++i) {
    auto next_groundness =
        static_cast<float>(i) / static_cast<float>(config.granularity);
    if (grounder.get_groundness() >= next_groundness) {
      continue;
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

    auto problem = grounder.extract_problem();

    try {
      auto encoder = SatPlanner::get_encoder(
          problem, config.grounding_timeout - timer.get_elapsed_time());
      auto encoding_size = encoder->get_universal_clauses().clauses.size() +
                           encoder->get_transition_clauses().clauses.size();
      LOG_INFO(engine_logger, "Encoding as %lu clauses", encoding_size);
      if (encoding_size < min_encoding_size) {
        min_encoding_size = encoding_size;
        smallest_encoder = std::move(encoder);
        smallest_problem = std::move(problem);
        min_grounding = grounder.get_groundness();
      }
    } catch (const TimeoutException &e) {
      break;
    }
  }

  LOG_INFO(engine_logger,
           "Smallest encoding with size %lu by problem with %.3f groundness",
           min_encoding_size, min_grounding);

  SatPlanner planner{};

  planner.set_encoder(std::move(smallest_encoder));

  LOG_INFO(engine_logger, "Planner started with no timeout");

  return planner.find_plan(smallest_problem, util::inf_time);
}
