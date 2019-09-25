#ifndef TOKENS_HPP
#define TOKENS_HPP

#include <string>

namespace parser {

namespace tokens {

struct LParen {
  static constexpr auto printable_name = "(";
  static constexpr auto primitive = "(";
};

struct RParen {
  static constexpr auto printable_name = ")";
  static constexpr auto primitive = ")";
};

struct Hyphen {
  static constexpr auto printable_name = "-";
  static constexpr auto primitive = "-";
};

struct Equality {
  static constexpr auto printable_name = "=";
  static constexpr auto primitive = "=";
};

struct And {
  static constexpr auto printable_name = "and";
  static constexpr auto primitive = "and";
};

struct Or {
  static constexpr auto printable_name = "or";
  static constexpr auto primitive = "or";
};

struct Not {
  static constexpr auto printable_name = "not";
  static constexpr auto primitive = "not";
};

struct Define {
  static constexpr auto printable_name = "define";
  static constexpr auto primitive = "define";
};

struct Domain {
  static constexpr auto printable_name = "domain";
  static constexpr auto primitive = "domain";
};

struct Problem {
  static constexpr auto printable_name = "problem";
  static constexpr auto primitive = "problem";
};

struct Increase {
  static constexpr auto printable_name = "increase";
  static constexpr auto primitive = "increase";
};

struct Decrease {
  static constexpr auto printable_name = "decrease";
  static constexpr auto primitive = "decrease";
};

struct Metric {
  static constexpr auto printable_name = "metric";
  static constexpr auto primitive = "metric";
};

struct Section {
  static constexpr auto printable_name = "Section";
  std::string name;
};

struct Identifier {
  static constexpr auto printable_name = "Identifier";
  std::string name;
};

struct Variable {
  static constexpr auto printable_name = "Variable";
  std::string name;
};

struct Number {
  static constexpr auto printable_name = "Number";
  int value;
};

struct Comment {
  static constexpr auto printable_name = "Comment";
  std::string content;
};

} // namespace tokens

} // namespace parser

#endif /* end of include guard: TOKENS_HPP */
