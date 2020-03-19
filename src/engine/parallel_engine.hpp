#ifndef PARALLEL_ENGINE_HPP
#define PARALLEL_ENGINE_HPP

#include "config.hpp"
#include "engine/engine.hpp"
#include "util/timer.hpp"

extern Config config;

class ParallelEngine final : public Engine {
public:
  explicit ParallelEngine(
      const std::shared_ptr<normalized::Problem> &problem) noexcept;

private:
  Plan start_planning_impl() override;
};

#endif /* end of include guard: PARALLEL_ENGINE_HPP */
