#ifndef LOGGING_HPP
#define LOGGING_HPP

#include "build_config.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

namespace logging {

enum class Level { ERROR, WARN, INFO, DEBUG };

inline constexpr const char *level_name(Level level) noexcept {
  switch (level) {
  case Level::ERROR:
    return "Error";
  case Level::WARN:
    return "Warn";
  case Level::INFO:
    return "Info";
  case Level::DEBUG:
    return "Debug";
  default:
    return "Unknown";
  }
}

enum class Color { Black = 30, Red, Green, Yellow, Blue, Magenta, Cyan, White };

class Appender {
public:
  Appender(const Appender &) = delete;
  Appender &operator=(const Appender &) = delete;

  virtual ~Appender() = default;

  void write(Level, const std::vector<char> &msg);
  inline void set_level(Level level) noexcept { level_ = level; }

protected:
  explicit Appender(Level, bool color_support) noexcept;

private:
  virtual void write_(const std::vector<char> &msg) = 0;

  Level level_;
  bool color_support_;
};

class ConsoleAppender : public Appender {
public:
  enum class Mode { Out, Err };

  explicit ConsoleAppender(Level = Level::INFO, Mode = Mode::Out) noexcept;

private:
  inline void write_(const std::vector<char> &msg) override {
    fprintf(stream_, "%s\n", msg.data());
  }

  inline static std::FILE *get_console(Mode mode) noexcept {
    return mode == Mode::Out ? stdout : stderr;
  }

  std::FILE *stream_;
};

class FileAppender : public Appender {
public:
  explicit FileAppender(Level, const std::filesystem::path &file,
                        bool append = false);

private:
  inline void write_(const std::vector<char> &msg) override {
    os_ << msg.data() << std::endl;
  }

  std::ofstream os_;
};

class Logger {
public:
  explicit Logger(const std::string &name) noexcept;
  Logger(Logger &&) = default;

  void log(Level, const char *file, unsigned int line, const char *format,
           ...) const;

  inline void add_appender(Appender &appender) noexcept {
    appenders_.push_back(&appender);
  }

private:
  std::vector<Appender *> appenders_;
  const std::string name_;
};

extern ConsoleAppender default_appender;
extern Logger default_logger;

#ifdef DEBUG_LOG
#define LOG_DEBUG(logger, ...)                                                 \
  logger.log(::logging::Level::DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define PRINT_DEBUG(...)                                                       \
  ::logging::default_logger.log(::logging::Level::DEBUG, __FILE__, __LINE__,   \
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
  logger.log(::logging::Level::INFO, "", 0, __VA_ARGS__)
#define PRINT_INFO(...)                                                        \
  ::logging::default_logger.log(::logging::Level::INFO, "", 0, __VA_ARGS__)
#define LOG_WARN(logger, ...)                                                  \
  logger.log(::logging::Level::WARN, "", 0, __VA_ARGS__)
#define PRINT_WARN(...)                                                        \
  ::logging::default_logger.log(::logging::Level::WARN, "", 0, __VA_ARGS__)
#define LOG_ERROR(logger, ...)                                                 \
  logger.log(::logging::Level::ERROR, "", 0, __VA_ARGS__)
#define PRINT_ERROR(...)                                                       \
  ::logging::default_logger.log(::logging::Level::ERROR, "", 0, __VA_ARGS__)

} // namespace logging

#endif /* end of include guard: LOGGING_HPP */
