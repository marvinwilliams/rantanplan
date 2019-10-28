#ifndef PREPROCESS_HPP
#define PREPROCESS_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/model.hpp"

namespace preprocess {

extern logging::Logger logger;

void preprocess(model::Problem &problem, const Config &config);

} // namespace preprocess

#endif /* end of include guard: PREPROCESS_HPP */
