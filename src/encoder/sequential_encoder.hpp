#ifndef SEQUENTIAL_ENCODER_HPP
#define SEQUENTIAL_ENCODER_HPP

#include "config.hpp"
#include "encoder/encoder.hpp"
#include "encoder/support.hpp"
#include "model/normalized/model.hpp"

#include <cstdint>
#include <vector>

class SequentialEncoder final : public Encoder {

public:
  explicit SequentialEncoder(const normalized::Problem &problem,
                             const Config &config) noexcept;

private:
  void encode_init() noexcept;
  void encode_actions() noexcept;
  void parameter_implies_predicate() noexcept;
  void interference() noexcept;
  void frame_axioms(unsigned int dnf_threshold) noexcept;
  void assume_goal() noexcept;
  void init_sat_vars() noexcept;

  uint_fast64_t num_vars_ = 3;
  std::vector<uint_fast64_t> predicates_;
  std::vector<uint_fast64_t> actions_;
  std::vector<std::vector<std::vector<uint_fast64_t>>> parameters_;

  Support support_;
};

#endif /* end of include guard: SEQUENTIAL_ENCODER_HPP */
