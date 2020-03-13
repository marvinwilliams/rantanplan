#include "sat/solver.hpp"
#include "util/timer.hpp"

#include <cassert>

namespace sat {

Solver &Solver::add(int l) {
  assert(status_ == Status::Constructing);
  add_impl(l);
  return *this;
}

Solver &Solver::operator<<(int l) { return add(l); }

void Solver::assume(int l) {
  assert(status_ == Status::Constructing);
  assume_impl(l);
}

void Solver::solve(util::Seconds timeout, util::Seconds solve_timeout) {
  assert(status_ == Status::Constructing);
  status_ = solve_impl(timeout, solve_timeout);
}

Solver::Status Solver::get_status() const { return status_; }

const Model &Solver::get_model() const {
  assert(status_ == Status::Solved);
  return model_;
}

} // namespace sat
