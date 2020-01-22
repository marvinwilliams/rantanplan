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
  enum class Status { Ready, Success, Timeout, Error };

  explicit Engine(const std::shared_ptr<normalized::Problem> &problem);

  void start();
  Status get_status() const;
  Plan get_plan() const;

  virtual ~Engine() = default;

protected:
  std::shared_ptr<normalized::Problem> problem_;
  Plan plan_;

private:
  virtual Status start_impl() = 0;
  Status status_ = Status::Ready;
};

#endif /* end of include guard: ENGINE_HPP */
