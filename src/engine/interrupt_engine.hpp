#ifndef INTERRUPT_ENGINE_HPP
#define INTERRUPT_ENGINE_HPP

#include "config.hpp"
#include "engine/engine.hpp"
#include "util/timer.hpp"

extern Config config;
extern util::Timer global_timer;

class InterruptEngine final : public Engine {
public:
  explicit InterruptEngine(
      const std::shared_ptr<normalized::Problem> &problem) noexcept;

private:
  Plan start_planning_impl() override;
};

#endif /* end of include guard: INTERRUPT_ENGINE_HPP */
