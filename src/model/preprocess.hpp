#ifndef PREPROCESS_HPP
#define PREPROCESS_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized_problem.hpp"

extern logging::Logger preprocess_logger;

void preprocess(normalized::Problem &problem, const Config& config);

#endif /* end of include guard: PREPROCESS_HPP */
