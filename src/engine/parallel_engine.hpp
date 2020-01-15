#ifndef PARALLEL_ENGINE_HPP
#define PARALLEL_ENGINE_HPP

#include "engine/engine.hpp"

class ParallelEngine final : public Engine {
public:
  explicit ParallelEngine(const std::shared_ptr<normalized::Problem> &problem,
                          const Config &config) noexcept;

private:
  Status start_impl() noexcept override;
};

#endif /* end of include guard: PARALLEL_ENGINE_HPP */
