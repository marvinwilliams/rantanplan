#ifndef LOCATION_HPP
#define LOCATION_HPP

#include <algorithm>
#include <ostream>
#include <string_view>

namespace lexer {

// A position represents one point in a file
class Position {
public:
  explicit Position() : Position(std::string_view{}) {}

  explicit Position(std::string_view filename, unsigned int line = 1u,
                    unsigned int column = 1u)
      : filename_{filename}, line_{line}, column_{column} {}

  std::string_view filename() const { return filename_; }

  unsigned int line() const { return line_; }

  unsigned int column() const { return column_; }

  void advance_line(unsigned int count = 1) {
    if (count != 0) {
      line_ += count;
      column_ = 1u;
    }
  }

  void advance_column(unsigned int count = 1) { column_ += count; }

private:
  std::string_view filename_;
  unsigned int line_;
  unsigned int column_;
};

inline Position &operator+=(Position &position, unsigned int count) {
  position.advance_column(count);
  return position;
}

inline Position operator+(Position position, unsigned int count) {
  return position += count;
}

inline bool operator==(const Position &first, const Position &second) {
  return first.filename() == second.filename() &&
         first.line() == second.line() && first.column() == second.column();
}

std::ostream &operator<<(std::ostream &out, const Position &position) {
  if (!position.filename().empty()) {
    out << position.filename() << ':';
  }
  out << position.line() << ':' << position.column();
  return out;
}

// A location is a sequence between a begin and an end position
class Location {
public:
  explicit Location(const Position &begin, const Position &end)
      : begin_{begin}, end_{end} {}

  explicit Location(const Position &begin) : begin_{begin}, end_{begin} {}

  explicit Location(const std::string &name) : Location{Position{name}} {}

  const Position &begin() const { return begin_; }

  const Position &end() const { return end_; }

  void step() { begin_ = end_; }

  void advance_line(unsigned int count = 1) { end_.advance_line(count); }

  void advance_column(unsigned int count = 1) { end_.advance_column(count); }

private:
  Position begin_;
  Position end_;
};

inline Location &operator+=(Location &location, unsigned int count) {
  location.advance_column(count);
  return location;
}

inline Location operator+(Location location, unsigned int count) {
  return location += count;
}

inline bool operator==(const Location &first, const Location &second) {
  return first.begin() == second.begin() && first.end() == second.end();
}

std::ostream &operator<<(std::ostream &out, const Location &location) {
  out << location.begin();
  if (location.begin().filename() != location.end().filename()) {
    out << '-' << location.end().filename() << ':' << location.end().line()
        << ':' << location.end().column() - 1;
  } else if (location.begin().line() != location.end().line()) {
    out << '-' << location.end().line() << ':' << location.end().column() - 1;
  } else if (location.begin().column() != location.end().column() - 1) {
    out << '-' << location.end().column() - 1;
  }
  return out;
}

} // namespace lexer

#endif /* end of include guard: LOCATION_HPP */
