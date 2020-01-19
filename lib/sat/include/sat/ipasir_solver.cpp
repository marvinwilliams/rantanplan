#include "sat/ipasir_solver.hpp"
#include "config.hpp"
#include "util/timer.hpp"

#include "ipasir.h"
#include <cassert>
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

namespace sat {

IpasirSolver::IpasirSolver(const Config &config) noexcept
    : handle_{ipasir_init()}, num_vars_{0}, config_{config} {
  ipasir_set_learn(handle_, NULL, 0, NULL);
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
  struct State {
    std::chrono::seconds timeout;
    util::Timer timer;
    const Config &config;
  };
  State state{timeout, {}, config_};
  ipasir_set_terminate(handle_, &state, [](void *state) {
    State *s = static_cast<State *>(state);
    if ((s->config.timeout > 0s &&
         std::chrono::ceil<std::chrono::seconds>(
             util::global_timer.get_elapsed_time()) >= s->config.timeout) ||
        (s->timeout > 0s && std::chrono::ceil<std::chrono::seconds>(
                                s->timer.get_elapsed_time()) >= s->timeout)) {
      return 1;
    }
    return 0;
  });
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
  } else if (result == 0) {
    return Status::Timeout;
  }
  return Status::Error;
}

} // namespace sat
