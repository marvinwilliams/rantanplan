#ifndef PREPROCESS_HPP
#define PREPROCESS_HPP

#include "model/model.hpp"
#include "model/support.hpp"

namespace model {

Support ground_rigid(float ratio, Problem &problem) noexcept;

Support preprocess(Problem &problem);

} // namespace model

#endif /* end of include guard: PREPROCESS_HPP */
