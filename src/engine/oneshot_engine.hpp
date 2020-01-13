#ifndef ONESHOT_ENGINE_HPP
#define ONESHOT_ENGINE_HPP

#include "engine/engine.hpp"

class OneshotEngine final : public Engine {
public:
  explicit OneshotEngine(const std::shared_ptr<normalized::Problem> &problem,
                         const Config &config) noexcept;

private:
  Status start_impl() noexcept override;
};

#endif /* end of include guard: ONESHOT_ENGINE_HPP */
