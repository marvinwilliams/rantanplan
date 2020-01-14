#ifndef INTERRUPT_ENGINE_HPP
#define INTERRUPT_ENGINE_HPP

#include "engine/engine.hpp"

class InterruptEngine final : public Engine {
public:
  explicit InterruptEngine(const std::shared_ptr<normalized::Problem> &problem,
                           const Config &config) noexcept;

private:
  Status start_impl() noexcept override;
};

#endif /* end of include guard: INTERRUPT_ENGINE_HPP */
