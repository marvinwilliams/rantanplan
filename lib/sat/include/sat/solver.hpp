#ifndef SOLVER_HPP
#define SOLVER_HPP

#include "sat/model.hpp"

#include <chrono>
#include <optional>
#include <vector>

namespace sat {

class Solver {
public:
  enum class Status { Constructing, Solved, Unsolvable, Timeout };

  Solver &add(int l);
  Solver &operator<<(int l);
  void assume(int l);
  void solve(std::chrono::seconds timeout);
  Status get_status() const;
  const Model &get_model() const;

  virtual ~Solver() = default;

protected:
  Status status_ = Status::Constructing;
  Model model_;

private:
  virtual void add_impl(int l) = 0;
  virtual void assume_impl(int l) = 0;
  virtual Status solve_impl(std::chrono::seconds timeout) = 0;
};

} // namespace sat

#endif /* end of include guard: SOLVER_HPP */
