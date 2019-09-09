#ifndef FOREACH_HPP
#define FOREACH_HPP

#include "sat/solver.hpp"
#include "model/problem.hpp"

namespace encoding {

void initial_state_clause(const model::Problem& problem, sat::Solver& solver);


}

#endif /* end of include guard: FOREACH_HPP */
