#ifndef FIXED_ENGINE_HPP
#define FIXED_ENGINE_HPP

#include "config.hpp"
#include "engine/engine.hpp"
#include "util/timer.hpp"

extern Config config;

class FixedEngine final : public Engine {
public:
  explicit FixedEngine(
      const std::shared_ptr<normalized::Problem> &problem) noexcept;

private:
  Plan start_planning_impl() override;
};

#endif /* end of include guard: FIXED_ENGINE_HPP */
