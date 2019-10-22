#ifndef PREPROCESS_HPP
#define PREPROCESS_HPP

#include "config.hpp"
#include "model/model.hpp"
#include "model/support.hpp"
#include "logging/logging.hpp"

namespace preprocess {

extern logging::Logger logger;

void preprocess(model::Problem &problem, support::Support &support, const Config& config);

} // namespace preprocess

#endif /* end of include guard: PREPROCESS_HPP */
