#ifndef IPASIR_SOLVER_HPP
#define IPASIR_SOLVER_HPP

#include "solver.hpp"

extern "C" {
#include "ipasir.h"
}

#include <cmath>
#include <iostream>
#include <vector>

namespace sat {

class IpasirSolver : public Solver {
public:
  IpasirSolver() : handle_{ipasir_init()}, num_vars{0} {
    ipasir_set_learn(handle_, NULL, 0, NULL);
  }

  IpasirSolver &operator<<(Literal literal) {
    ipasir_add(handle_, literal.index);
    if (std::abs(literal.index) > static_cast<int>(num_vars)) {
      num_vars = static_cast<unsigned int>(std::abs(literal.index));
    }
    return *this;
  }

  void assume(Literal literal) { ipasir_assume(handle_, literal.index); }

  std::optional<Model> solve(std::chrono::milliseconds timeout) {
    using clock = std::chrono::steady_clock;
    auto end_time = clock::now() + timeout;
    if (timeout != timeout.zero()) {
      ipasir_set_terminate(handle_, &end_time, [](void *end_time) {
        return static_cast<int>(std::max(
            (clock::now() - *static_cast<clock::time_point *>(end_time))
                .count(),
            clock::rep(0)));
      });
    }
    if (ipasir_solve(handle_) == 10) {
      Model model;
      model.assignment.reserve(num_vars + 1);
      model.assignment.push_back(false); // Skip index 0
      for (unsigned int i = 1; i <= num_vars; ++i) {
        int index = static_cast<int>(i);
        model.assignment.push_back(ipasir_val(handle_, index) == index);
      }
      return model;
    }
    return std::nullopt;
  }

  ~IpasirSolver() { ipasir_release(handle_); }

private:
  void *handle_;
  unsigned int num_vars;
};

} // namespace sat

#endif /* end of include guard: IPASIR_SOLVER_HPP */
