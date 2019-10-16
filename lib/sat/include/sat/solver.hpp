#ifndef SOLVER_HPP
#define SOLVER_HPP

#include "sat/model.hpp"

#include <chrono>
#include <optional>
#include <vector>

namespace sat {

class Solver {
public:
  virtual Solver &operator<<(int) noexcept = 0;

  virtual void assume(int) noexcept = 0;
  virtual std::optional<Model>
  solve(std::chrono::milliseconds timeout = std::chrono::seconds{0}) = 0;

  virtual ~Solver() noexcept = default;

protected:
  Solver() = default;
  Solver(const Solver &solver) = delete;
  Solver &operator=(const Solver &solver) = delete;
};

} // namespace sat

#endif /* end of include guard: SOLVER_HPP */
