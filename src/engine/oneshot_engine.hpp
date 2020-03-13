#ifndef ONESHOT_ENGINE_HPP
#define ONESHOT_ENGINE_HPP

#include "config.hpp"
#include "engine/engine.hpp"
#include "util/timer.hpp"

extern Config config;

class OneshotEngine final : public Engine {
public:
  explicit OneshotEngine(
      const std::shared_ptr<normalized::Problem> &problem) noexcept;

private:
  Plan start_planning_impl() override;
};

#endif /* end of include guard: ONESHOT_ENGINE_HPP */
