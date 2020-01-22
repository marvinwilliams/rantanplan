#ifndef FOREACH_ENCODER_HPP
#define FOREACH_ENCODER_HPP

#include "config.hpp"
#include "encoder/encoder.hpp"
#include "encoder/support.hpp"
#include "model/normalized/model.hpp"

#include <cstdint>
#include <vector>

extern Config config;

class ForeachEncoder final : public Encoder {
public:
  explicit ForeachEncoder(
      const std::shared_ptr<normalized::Problem> &problem) noexcept;

  int to_sat_var(Literal l, unsigned int step) const noexcept override;
  Plan extract_plan(const sat::Model &model, unsigned int num_steps) const
      noexcept override;

private:
  void encode_init() noexcept;
  void encode_actions() noexcept;
  void parameter_implies_predicate() noexcept;
  void interference() noexcept;
  uint_fast64_t frame_axioms() noexcept;
  void assume_goal() noexcept;
  void init_sat_vars() noexcept;

  uint_fast64_t num_vars_ = 3;
  std::vector<uint_fast64_t> predicates_;
  std::vector<uint_fast64_t> actions_;
  std::vector<std::vector<std::vector<uint_fast64_t>>> parameters_;

  Support support_;
};

#endif /* end of include guard: FOREACH_ENCODER_HPP */
