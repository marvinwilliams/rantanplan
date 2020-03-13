#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>
#include <exception>
#include <string>

struct TimeoutException : std::exception {
  inline const char *what() const noexcept override { return "timeout"; }
};

namespace util {

using Seconds = std::chrono::duration<float>;
static constexpr Seconds inf_time =
    Seconds{std::numeric_limits<float>::infinity()};

struct Timer {
  const std::chrono::steady_clock::time_point start_time =
      std::chrono::steady_clock::now();

  auto get_elapsed_time() const {
    return std::chrono::steady_clock::now() - start_time;
  }
};

} // namespace util

#endif /* end of include guard: TIMER_HPP */
