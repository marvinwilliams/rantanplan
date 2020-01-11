#include <chrono>

namespace util {

struct Timer {
  const std::chrono::steady_clock::time_point start_time =
      std::chrono::steady_clock::now();

  double get_elapsed_time() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                         start_time)
        .count();
  }
};

inline static const Timer global_timer;

} // namespace util
