#ifndef IPASIR_SOLVER_HPP
#define IPASIR_SOLVER_HPP

#include "config.hpp"
#include "sat/model.hpp"
#include "sat/solver.hpp"
#include "util/timer.hpp"

extern "C" {
#include "ipasir.h"
}

#include <chrono>

extern Config config;

namespace sat {

class IpasirSolver final : public Solver {
public:
  explicit IpasirSolver() noexcept;

  IpasirSolver(const IpasirSolver &) = delete;
  IpasirSolver &operator=(const IpasirSolver &) = delete;

  ~IpasirSolver() noexcept;

  void next_step() noexcept;

private:
  void add_impl(int l) noexcept override;
  void assume_impl(int l) noexcept override;
  Status solve_impl(std::chrono::seconds timeout) noexcept override;

  void *handle_ = nullptr;
  unsigned int num_vars_ = 0;
};

} // namespace sat

#endif /* end of include guard: IPASIR_SOLVER_HPP */
