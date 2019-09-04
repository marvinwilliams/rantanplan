#ifndef INTERFACE_HPP
#define INTERFACE_HPP

extern "C" {
#include "ipasir.h"
}
#include <iostream>
#include <vector>

namespace sat {

class Interface {
public:
  Interface() : solver_{ipasir_init()} {
    ipasir_set_learn(solver_, NULL, 0, NULL);
  }

  void add(int literal) { ipasir_add(solver_, literal); }

  void assume(const std::vector<int> &literals) {
    for (const int l : literals) {
      ipasir_assume(solver_, l);
    }
  }

  bool solve() { return ipasir_solve(solver_) == 10; }

  int get_value(int literal) { return ipasir_val(solver_, literal); }

  ~Interface() { ipasir_release(solver_); }

private:
  void *solver_;
};

} // namespace sat

#endif /* end of include guard: INTERFACE_HPP */
