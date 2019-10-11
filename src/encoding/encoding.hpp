#ifndef ENCODING_HPP
#define ENCODING_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/model.hpp"
#include "model/preprocess.hpp"
#include "model/support.hpp"
#include "model/to_string.hpp"
#include "sat/formula.hpp"
#include "sat/ipasir_solver.hpp"

#include <variant>
#include <vector>

namespace encoding {

using namespace std::chrono_literals;

extern logging::Logger logger;

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

  explicit Encoder(const model::Support &support)
      : solver_{std::make_unique<sat::IpasirSolver>()}, support_{support} {}

  virtual ~Encoder() = default;

  void encode() {
    generate_formula_();
    init_vars_();
  }

  virtual void plan(const Config &) = 0;

protected:
  Encoder(const Encoder &) = delete;
  Encoder(Encoder &&) = default;
  Encoder &operator=(const Encoder &) = delete;
  Encoder &operator=(Encoder &&) = delete;

  std::unique_ptr<sat::Solver> solver_;

  model::Support support_;

private:
  virtual void generate_formula_() = 0;
  virtual void init_vars_() = 0;
}; // namespace encoding

} // namespace encoding

#endif /* end of include guard: ENCODING_HPP */
