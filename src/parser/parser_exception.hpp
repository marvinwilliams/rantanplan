#ifndef PARSER_EXCEPTION_HPP
#define PARSER_EXCEPTION_HPP

#include "lexer/location.hpp"

#include <exception>
#include <optional>
#include <string>

namespace parser {

class ParserException : public std::exception {
public:
  explicit ParserException(const lexer::Location &location, std::string message)
      : location_(location), message_{std::move(message)} {}
  explicit ParserException(const lexer::Location &location)
      : ParserException(location, "unknown error") {}
  explicit ParserException(std::string message)
      : message_{std::move(message)} {}

  [[nodiscard]] inline const char *what() const noexcept override {
    return message_.c_str();
  }

  [[nodiscard]] inline const std::optional<lexer::Location> &location() const
      noexcept {
    return location_;
  }

private:
  std::optional<lexer::Location> location_;
  std::string message_;
};

} // namespace parser

#endif /* end of include guard: PARSER_EXCEPTION_HPP */
