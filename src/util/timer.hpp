#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>

namespace util {

struct Timer {
  const std::chrono::steady_clock::time_point start_time =
      std::chrono::steady_clock::now();

  std::chrono::steady_clock::duration get_elapsed_time() const {
    return std::chrono::steady_clock::now() - start_time;
  }
};

inline static const Timer global_timer;

} // namespace util

#endif /* end of include guard: TIMER_HPP */
