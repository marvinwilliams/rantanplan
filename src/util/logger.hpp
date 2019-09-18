#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "config.hpp"
#include <chrono>
#include <cstdarg>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace logging {

static const auto start_time = std::chrono::steady_clock::now();

enum class Level { ERROR, WARN, INFO, DEBUG };
enum class Color { Black = 30, Red, Green, Yellow, Blue, Magenta, Cyan, White };

bool is_tty(std::FILE *stream) { return isatty(fileno(stream)) != 0; }

class Appender {
public:
  void write(Level level, const std::vector<char> &msg) {
    if (level <= level_) {
      if (color_support_ && level <= Level::WARN) {
        auto color_message = [&msg](char *buffer, size_t length, Color color) {
          return std::snprintf(buffer, length, "\033[%dm%s\033[0m",
                               static_cast<int>(color), msg.data());
        };
        std::vector<char> c_msg;
        if (level == Level::WARN) {
          auto c_length = static_cast<unsigned int>(
              color_message(nullptr, 0, Color::Yellow));
          c_msg.resize(c_length + 1);
          color_message(c_msg.data(), c_msg.size(), Color::Yellow);
        } else {
          auto c_length =
              static_cast<unsigned int>(color_message(nullptr, 0, Color::Red));
          c_msg.resize(c_length + 1);
          color_message(c_msg.data(), c_msg.size(), Color::Red);
        }
        write(c_msg);
      } else {
        write(msg);
      }
    }
  }

  void set_level(Level level) { level_ = level; }

  virtual ~Appender() {}

protected:
  Appender(Level level, bool color_support)
      : level_{level}, color_support_{color_support} {}

private:
  virtual void write(const std::vector<char> &) = 0;
  Level level_;
  bool color_support_;
}; // namespace logging

class ConsoleAppender : public Appender {
public:
  ConsoleAppender(const ConsoleAppender &) = delete;
  ConsoleAppender &operator=(const ConsoleAppender &) = delete;
  enum class Mode { Out, Err };

  ConsoleAppender(Level level = Level::INFO, Mode mode = Mode::Out)
      : Appender{level, is_tty(get_console(mode))}, stream_{get_console(mode)} {
  }

private:
  void write(const std::vector<char> &msg) override {
    fprintf(stream_, "%s\n", msg.data());
  }
  static std::FILE *get_console(Mode mode) {
    return mode == Mode::Out ? stdout : stderr;
  }

  std::FILE *stream_;
};

class FileAppender : public Appender {
public:
  FileAppender(Level level, const std::filesystem::path &file,
               bool append = false)
      : Appender{level, false}, os_{file,
                                    append ? std::ios::app : std::ios::out} {
    os_.exceptions(std::ios::failbit);
  }

private:
  void write(const std::vector<char> &msg) override {
    os_ << msg.data() << std::endl;
  }

  std::ofstream os_;
};

static Appender &get_default_appender() {
  static ConsoleAppender default_appender{debug_mode ? Level::DEBUG
                                                     : Level::INFO};
  return default_appender;
}

class Logger {
public:
  Logger(const std::string &name) : name_{name} {}

  Logger(const std::string &name, Appender &appender) : name_{name} {
    add_appender(appender);
  }

  void log(Level level, const char *file, unsigned int line, const char *format,
           ...) const {
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
                        level_names_[static_cast<unsigned int>(level)],
                        msg_buffer.data());
      } else {
        return snprintf(
            buffer, length, "%s (%.3f) %s:%u %s [%s]: %s", time_buffer.data(),
            uptime, std::filesystem::relative(file).c_str(), line, name_.data(),
            level_names_[static_cast<unsigned int>(level)], msg_buffer.data());
      }
    };
    auto result_length = static_cast<unsigned int>(format_message(nullptr, 0));
    std::vector<char> result_buffer(result_length + 1);
    format_message(result_buffer.data(), result_buffer.size());
    for (Appender *appender : appenders_) {
      appender->write(level, result_buffer);
    }
  }

  void add_appender(Appender &appender) { appenders_.push_back(&appender); }

  void add_default_appender() { add_appender(get_default_appender()); }

private:
  std::vector<const char *> level_names_ = {"Error", "Warn", "Info", "Debug"};
  std::vector<Appender *> appenders_;
  std::string name_;
};

static Logger &get_default_logger() {
  static Logger default_logger{"Main", get_default_appender()};
  return default_logger;
}

#ifdef DEBUG_PRINT
#define LOG_DEBUG(logger, ...)                                                 \
  logger.log(logging::Level::DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define PRINT_DEBUG(...)                                                       \
  logging::get_default_logger().log(logging::Level::DEBUG, __FILE__, __LINE__, \
                                    __VA_ARGS__)
#else
#define LOG_DEBUG(logger, ...)                                                 \
  do {                                                                         \
  } while (false)
#define PRINT_DEBUG(...)                                                       \
  do {                                                                         \
  } while (false)
#endif
#define LOG_INFO(logger, ...)                                                  \
  logger.log(logging::Level::INFO, "", 0, __VA_ARGS__)
#define PRINT_INFO(...)                                                        \
  logging::get_default_logger().log(logging::Level::INFO, "", 0, __VA_ARGS__)
#define LOG_WARN(logger, ...) logger.log(logging::Level::WARN, __VA_ARGS__)
#define PRINT_WARN(...)                                                        \
  logging::get_default_logger().log(logging::Level::WARN, "", 0, __VA_ARGS__)
#define LOG_ERROR(logger, ...)                                                 \
  logger.log(logging::Level::ERROR, "", 0, __VA_ARGS__)
#define PRINT_ERROR(...)                                                       \
  logging::get_default_logger().log(logging::Level::ERROR, "", 0, __VA_ARGS__)

} // namespace logging

#endif /* end of include guard: LOGGER_HPP */
