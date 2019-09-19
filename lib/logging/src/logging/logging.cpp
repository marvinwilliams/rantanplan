#include "build_config.hpp"
#include "logging/logging.hpp"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>

namespace logging {

ConsoleAppender default_appender{DEBUG_MODE ? Level::DEBUG : Level::INFO};
Logger default_logger = []() {
  Logger logger{"Main"};
  logger.add_appender(default_appender);
  return logger;
}();

static const auto start_time = std::chrono::steady_clock::now();

inline static bool is_tty(std::FILE *stream) noexcept {
  return isatty(fileno(stream)) != 0;
}

Appender::Appender(Level level, bool color_support) noexcept
    : level_{level}, color_support_{color_support} {}

void Appender::write(Level level, const std::vector<char> &msg) {
  if (level <= level_) {
    if (color_support_ && level <= Level::WARN) {
      auto color_message = [&msg](char *buffer, size_t length, Color color) {
        return std::snprintf(buffer, length, "\033[%dm%s\033[0m",
                             static_cast<int>(color), msg.data());
      };
      std::vector<char> c_msg;
      if (level == Level::WARN) {
        auto c_length =
            static_cast<unsigned int>(color_message(nullptr, 0, Color::Yellow));
        c_msg.resize(c_length + 1);
        color_message(c_msg.data(), c_msg.size(), Color::Yellow);
      } else {
        auto c_length =
            static_cast<unsigned int>(color_message(nullptr, 0, Color::Red));
        c_msg.resize(c_length + 1);
        color_message(c_msg.data(), c_msg.size(), Color::Red);
      }
      write_(c_msg);
    } else {
      write_(msg);
    }
  }
}

ConsoleAppender::ConsoleAppender(Level level, Mode mode) noexcept
    : Appender{level, is_tty(get_console(mode))}, stream_{get_console(mode)} {}

FileAppender::FileAppender(Level level, const std::filesystem::path &file,
                           bool append)
    : Appender{level, false}, os_{file,
                                  append ? std::ios::app : std::ios::out} {
  os_.exceptions(std::ios::failbit);
}

Logger::Logger(const std::string &name) noexcept : name_{name} {}

void Logger::log(Level level, const char *file, unsigned int line,
                 const char *format, ...) const {
  if (appenders_.empty()) {
    return;
  }
  const auto uptime = std::chrono::duration<double>(
                          std::chrono::steady_clock::now() - start_time)
                          .count();
  va_list args;
  va_start(args, format);
  va_list args_copy;
  va_copy(args_copy, args);
  auto msg_length =
      static_cast<unsigned int>(std::vsnprintf(nullptr, 0, format, args));
  va_end(args);
  std::vector<char> msg_buffer(msg_length + 1);
  std::vsnprintf(msg_buffer.data(), msg_buffer.size(), format, args_copy);
  va_end(args_copy);
  std::vector<char> time_buffer(std::strlen("YYYY-MM-DD HH:MM:SS") + 1);
  std::time_t time = std::time(nullptr);
  std::strftime(time_buffer.data(), time_buffer.size(), "%F %T",
                std::localtime(&time));
  auto format_message = [&](char *buffer, size_t length) {
    if (line == 0) {
      return snprintf(buffer, length, "%s (%.3f) %s [%s]: %s",
                      time_buffer.data(), uptime, name_.data(),
                      level_name(level), msg_buffer.data());
    } else {
      return snprintf(buffer, length, "%s (%.3f) %s:%u %s [%s]: %s",
                      time_buffer.data(), uptime,
                      std::filesystem::relative(file).c_str(), line,
                      name_.data(), level_name(level), msg_buffer.data());
    }
  };
  auto result_length = static_cast<unsigned int>(format_message(nullptr, 0));
  std::vector<char> result_buffer(result_length + 1);
  format_message(result_buffer.data(), result_buffer.size());
  for (Appender *appender : appenders_) {
    appender->write(level, result_buffer);
  }
}

} // namespace logging
