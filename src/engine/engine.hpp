#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "config.hpp"
#include "model/normalized/model.hpp"
#include "planner/sat_planner.hpp"

#include <memory>

class Engine {
public:
  enum class Status { Ready, Success, Timeout, Error };

  explicit Engine(const std::shared_ptr<normalized::Problem> &problem,
                  const Config &config);

  void start();
  Status get_status() const;
  Plan get_plan() const;

  virtual ~Engine() = default;

protected:
  const Config& config_;
  std::shared_ptr<normalized::Problem> problem_;
  Plan plan_;
  Status status_ = Status::Ready;

private:
  virtual Status start_impl() = 0;
};

#endif /* end of include guard: ENGINE_HPP */
