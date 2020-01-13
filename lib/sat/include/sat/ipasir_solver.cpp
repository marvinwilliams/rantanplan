#include "sat/ipasir_solver.hpp"
#include "ipasir.h"
#include <cassert>

namespace sat {

IpasirSolver::IpasirSolver() noexcept : handle_{ipasir_init()}, num_vars_{0} {
  ipasir_set_learn(handle_, NULL, 0, NULL);
}

IpasirSolver::IpasirSolver(IpasirSolver &&other) noexcept
    : Solver(std::move(other)), handle_{std::exchange(other.handle_, nullptr)},
      num_vars_{std::exchange(other.num_vars_, 0)} {}

IpasirSolver &IpasirSolver::operator=(IpasirSolver &&other) noexcept {
  Solver::operator=(std::move(other));
  if (handle_ != nullptr) {
    ipasir_release(handle_);
  }
  handle_ = std::exchange(other.handle_, nullptr);
  num_vars_ = std::exchange(other.num_vars_, 0);
  return *this;
}

IpasirSolver::~IpasirSolver() noexcept { ipasir_release(handle_); }

void IpasirSolver::next_step() noexcept {
  model_.assignment.clear();
  status_ = Status::Constructing;
}

void IpasirSolver::add_impl(int l) noexcept {
  ipasir_add(handle_, l);
  num_vars_ = std::max(num_vars_, static_cast<unsigned int>(std::abs(l)));
}

void IpasirSolver::assume_impl(int l) noexcept { ipasir_assume(handle_, l); }

Solver::Status IpasirSolver::solve_impl(std::chrono::seconds timeout) noexcept {
  using clock = std::chrono::steady_clock;
  auto end_time = clock::time_point{};
  if (timeout != clock::duration::zero()) {
    end_time = clock::now() + timeout;
    ipasir_set_terminate(handle_, &end_time, [](void *end_time) {
      auto remaining =
          clock::now() - *static_cast<clock::time_point *>(end_time);
      return remaining > clock::duration::zero() ? 0 : 1;
    });
  } else {
    ipasir_set_terminate(handle_, nullptr, [](void *) { return 0; });
  }
  if (int result = ipasir_solve(handle_); result == 10) {
    model_.assignment.clear();
    model_.assignment.reserve(num_vars_ + 1);
    model_.assignment.push_back(false); // Skip index 0
    for (unsigned int i = 1; i <= num_vars_; ++i) {
      int index = static_cast<int>(i);
      model_.assignment.push_back(ipasir_val(handle_, index) == index);
    }
    return Status::Solved;
  } else if (result == 20) {
    return Status::Unsolvable;
  }
  return Status::Timeout;
}

} // namespace sat
