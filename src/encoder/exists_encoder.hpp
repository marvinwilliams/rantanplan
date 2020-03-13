#ifndef EXISTS_ENCODER_HPP
#define EXISTS_ENCODER_HPP

#include "config.hpp"
#include "encoder/encoder.hpp"
#include "encoder/support.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"

#include <cstdint>
#include <map>
#include <vector>

extern Config config;

class ExistsEncoder final : public Encoder {
public:
  explicit ExistsEncoder(const std::shared_ptr<normalized::Problem> &problem);

  int to_sat_var(Literal l, unsigned int step) const noexcept override;
  Plan extract_plan(const sat::Model &model, unsigned int num_steps) const
      noexcept override;

private:
  size_t get_constant_index(normalized::ConstantIndex constant,
                            normalized::TypeIndex type) const noexcept;
  void encode_init();
  void encode_actions();
  void parameter_implies_predicate();
  void interference();
  void frame_axioms();
  void assume_goal();
  void init_sat_vars();

  uint_fast64_t num_vars_ = 3;
  std::vector<uint_fast64_t> action_rank_;
  std::vector<uint_fast64_t> predicates_;
  std::vector<uint_fast64_t> actions_;
  std::vector<std::vector<std::vector<uint_fast64_t>>> parameters_;
  std::vector<std::unordered_map<normalized::ActionIndex, uint_fast64_t>>
      pos_helpers_;
  std::vector<std::unordered_map<normalized::ActionIndex, uint_fast64_t>>
      neg_helpers_;
  std::vector<std::unordered_map<normalized::ParameterAssignment, uint_fast64_t>>
      dnf_helpers_;

  Support support_;
};

#endif /* end of include guard: EXISTS_ENCODER_HPP */
