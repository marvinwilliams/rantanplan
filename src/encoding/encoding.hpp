#ifndef ENCODING_HPP
#define ENCODING_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/model.hpp"
#include "model/support.hpp"
#include "sat/ipasir_solver.hpp"

#include <variant>
#include <vector>

namespace planning {

  struct ActionVariable {
    model::ActionPtr action_ptr;
  };

  struct PredicateVariable {
    model::GroundPredicatePtr predicate_ptr;
    bool this_step;
  };

  struct ParameterVariable {
    model::ActionPtr action_ptr;
    size_t parameter_index;
    size_t constant_index;
  };

  struct GlobalParameterVariable {
    size_t parameter_index;
    size_t constant_index;
  };

class Encoder {

public:
  using Plan =
      std::vector<std::pair<model::ActionPtr, std::vector<model::ConstantPtr>>>;

  Encoder(const Encoder &) = delete;
  Encoder &operator=(const Encoder &) = delete;
  explicit Encoder() : solver_{std::make_unique<sat::IpasirSolver>()} {}
  virtual ~Encoder() = default;

  void encode(const model::Problem &problem) {
    generate_formula_();
    init_vars_();
  }

  virtual void plan(const Config &) = 0;

protected:
  Encoder(Encoder &&) = default;
  Encoder &operator=(Encoder &&) = default;
  std::unique_ptr<sat::Solver> solver_;

private:
  virtual void generate_formula_() = 0;
  virtual void init_vars_() = 0;
};

} // namespace planning

#endif /* end of include guard: ENCODING_HPP */
