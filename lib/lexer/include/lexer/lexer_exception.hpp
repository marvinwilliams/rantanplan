#ifndef LEXER_EXCEPTION_HPP
#define LEXER_EXCEPTION_HPP

#include "lexer/location.hpp"

#include <exception>
#include <optional>
#include <string>

namespace lexer {

class LexerException : public std::exception {
public:
  LexerException(const Location &location, const std::string &message)
      : location_(location), message_{message} {}
  LexerException(const Location &location)
      : LexerException(location, "unknown error") {}
  LexerException(const std::string &message) : message_{message} {}

  const char *what() const noexcept override { return message_.c_str(); }

  std::optional<Location> location() const noexcept { return location_; }

private:
  std::optional<Location> location_;
  std::string message_;
};

} // namespace lexer

#endif /* end of include guard: LEXER_EXCEPTION_HPP */
