#ifndef LEXER_EXCEPTION_HPP
#define LEXER_EXCEPTION_HPP

#include "lexer/location.hpp"

#include <exception>
#include <optional>
#include <string>

namespace lexer {

class LexerException : public std::exception {
public:
  explicit LexerException(const Location &location, std::string message)
      : location_(location), message_{std::move(message)} {}
  explicit LexerException(const Location &location)
      : LexerException(location, "unknown error") {}
  explicit LexerException(std::string message) : message_{std::move(message)} {}

  inline const char *what() const noexcept override { return message_.c_str(); }

  inline const std::optional<Location> &location() const noexcept {
    return location_;
  }

private:
  std::optional<Location> location_;
  std::string message_;
};

} // namespace lexer

#endif /* end of include guard: LEXER_EXCEPTION_HPP */
