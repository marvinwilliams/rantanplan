#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "planner/sat_planner.hpp"

#include <memory>

extern logging::Logger engine_logger;

class Engine {
public:
  explicit Engine(const std::shared_ptr<normalized::Problem> &problem);

  Plan start_planning();

  virtual ~Engine() = default;

protected:
  std::shared_ptr<normalized::Problem> problem_;

private:
  virtual Plan start_planning_impl() = 0;
};

#endif /* end of include guard: ENGINE_HPP */
