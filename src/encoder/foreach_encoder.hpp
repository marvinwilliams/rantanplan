#ifndef FOREACH_ENCODER_HPP
#define FOREACH_ENCODER_HPP

#include "config.hpp"
#include "encoder/encoder.hpp"
#include "encoder/support.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include <set>

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
  size_t get_assignment_id(const normalized::ParameterAssignment &assignment,
                           const normalized::Problem &problem) const noexcept;
  size_t
  get_selection_index(size_t action_index,
                      const normalized::ParameterAssignment &assignment) const
      noexcept;
  bool encode_init() noexcept;
  bool encode_actions() noexcept;
  bool parameter_implies_predicate() noexcept;
  bool interference() noexcept;
  bool frame_axioms() noexcept;
  bool assume_goal() noexcept;
  bool init_sat_vars() noexcept;

  uint_fast64_t num_vars_ = 3;
  uint_fast64_t num_helpers_ = 0;
  std::vector<uint_fast64_t> predicates_;
  std::vector<uint_fast64_t> actions_;
  std::vector<std::vector<std::unordered_map<size_t, uint_fast64_t>>>
      assignments_;
  std::vector<std::vector<normalized::ParameterSelection>> selections_;

  Support support_;
};

#endif /* end of include guard: FOREACH_ENCODER_HPP */
