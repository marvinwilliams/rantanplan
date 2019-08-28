#ifndef PARSER_EXCEPTION_HPP
#define PARSER_EXCEPTION_HPP

#include "lexer/location.hpp"

#include <exception>
#include <optional>
#include <string>

namespace parser {

class ParserException : public std::exception {
public:
  ParserException(const lexer::Location &location, const std::string &message)
      : location_(location), message_{message} {}
  ParserException(const lexer::Location &location)
      : ParserException(location, "unknown error") {}
  ParserException(const std::string &message) : message_{message} {}

  const char *what() const noexcept override { return message_.c_str(); }

  std::optional<lexer::Location> location() const noexcept { return location_; }

private:
  std::optional<lexer::Location> location_;
  std::string message_;
};
} // namespace parser

#endif /* end of include guard: PARSER_EXCEPTION_HPP */
