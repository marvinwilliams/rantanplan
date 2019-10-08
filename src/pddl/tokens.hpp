#ifndef TOKENS_HPP
#define TOKENS_HPP

#include "lexer/lexer_new.hpp"
#include "lexer/rules.hpp"
#include "lexer/token_set.hpp"

#include <cassert>
#include <string_view>

namespace pddl {

namespace token {

struct LParen : lexer::rule::Literal<'('> {
  static constexpr auto printable_name = "(";
};

struct RParen : lexer::rule::Literal<')'> {
  static constexpr auto printable_name = ")";
};

struct Hyphen : lexer::rule::Literal<'-'> {
  static constexpr auto printable_name = "-";
};

struct Equality : lexer::rule::Literal<'='> {
  static constexpr auto printable_name = "-";
};

struct And : lexer::rule::IWord<'a', 'n', 'd'> {
  static constexpr auto printable_name = "and";
};

struct Or : lexer::rule::IWord<'o', 'r'> {
  static constexpr auto printable_name = "or";
};

struct Not : lexer::rule::IWord<'n', 'o', 't'> {
  static constexpr auto printable_name = "not";
};

struct Define : lexer::rule::Word<'d', 'e', 'f', 'i', 'n', 'e'> {
  static constexpr auto printable_name = "define";
};

struct Domain : lexer::rule::Word<'d', 'o', 'm', 'a', 'i', 'n'> {
  static constexpr auto printable_name = "domain";
};

struct Problem : lexer::rule::Word<'p', 'r', 'o', 'b', 'l', 'e', 'm'> {
  static constexpr auto printable_name = "problem";
};

struct Increase : lexer::rule::Word<'i', 'n', 'c', 'r', 'e', 'a', 's', 'e'> {
  static constexpr auto printable_name = "increase";
};

struct Decrease : lexer::rule::Word<'d', 'e', 'c', 'r', 'e', 'a', 's', 'e'> {
  static constexpr auto printable_name = "decrease";
};

struct Minimize : lexer::rule::Word<'m', 'i', 'n', 'i', 'm', 'i', 'z', 'e'> {
  static constexpr auto printable_name = "minimize";
};

struct Maximize : lexer::rule::Word<'m', 'a', 'x', 'i', 'm', 'i', 'z', 'e'> {
  static constexpr auto printable_name = "maximize";
};

struct Metric : lexer::rule::Word<':', 'm', 'e', 't', 'r', 'i', 'c'> {
  static constexpr auto printable_name = "metric";
};

struct Requirements : lexer::rule::Word<':', 'r', 'e', 'q', 'u', 'i', 'r', 'e',
                                        'm', 'e', 'n', 't', 's'> {
  static constexpr auto printable_name = "requirements";
};

struct Types : lexer::rule::Word<':', 't', 'y', 'p', 'e', 's'> {
  static constexpr auto printable_name = "types";
};

struct Constants
    : lexer::rule::Word<':', 'c', 'o', 'n', 's', 't', 'a', 'n', 't', 's'> {
  static constexpr auto printable_name = "constants";
};

struct Predicates
    : lexer::rule::Word<':', 'p', 'r', 'e', 'd', 'i', 'c', 'a', 't', 'e', 's'> {
  static constexpr auto printable_name = "predicates";
};

struct Functions
    : lexer::rule::Word<':', 'f', 'u', 'n', 'c', 't', 'i', 'o', 'n', 's'> {
  static constexpr auto printable_name = "functions";
};

struct Action : lexer::rule::Word<':', 'a', 'c', 't', 'i', 'o', 'n'> {
  static constexpr auto printable_name = "action";
};

struct Parameters
    : lexer::rule::Word<':', 'p', 'a', 'r', 'a', 'm', 'e', 't', 'e', 'r', 's'> {
  static constexpr auto printable_name = "parameters";
};

struct Precondition : lexer::rule::Word<':', 'p', 'r', 'e', 'c', 'o', 'n', 'd',
                                        'i', 't', 'i', 'o', 'n'> {
  static constexpr auto printable_name = "precondition";
};

struct Effect : lexer::rule::Word<':', 'e', 'f', 'f', 'e', 'c', 't'> {
  static constexpr auto printable_name = "effect";
};

struct DomainRef : lexer::rule::Word<':', 'd', 'o', 'm', 'a', 'i', 'n'> {
  static constexpr auto printable_name = "domain_ref";
};

struct Objects : lexer::rule::Word<':', 'o', 'b', 'j', 'e', 'c', 't', 's'> {
  static constexpr auto printable_name = "objects";
};

struct Init : lexer::rule::Word<':', 'i', 'n', 'i', 't'> {
  static constexpr auto printable_name = "init";
};

struct Goal : lexer::rule::Word<':', 'g', 'o', 'a', 'l'> {
  static constexpr auto printable_name = "goal";
};

struct Name
    : lexer::rule::Sequence<lexer::rule::Alpha,
                            lexer::rule::Star<lexer::rule::Choice<
                                lexer::rule::Alnum, lexer::rule::Literal<'-'>,
                                lexer::rule::Literal<'_'>>>> {
  static constexpr auto printable_name = "<name>";
  std::string_view name;
};

struct Requirement : lexer::rule::Sequence<lexer::rule::Literal<':'>, Name> {
  static constexpr auto printable_name = "<requirement>";
  std::string_view name;
};

struct Variable : lexer::rule::Sequence<lexer::rule::Literal<'?'>, Name> {
  static constexpr auto printable_name = "<variable>";
  std::string_view name;
};

struct Number
    : lexer::rule::Sequence<
          lexer::rule::Optional<lexer::rule::Choice<lexer::rule::Literal<'+'>,
                                                    lexer::rule::Literal<'-'>>>,
          lexer::rule::Plus<lexer::rule::Digit>> {
  static constexpr auto printable_name = "<number>";
  int value;
};

struct Comment : lexer::rule::Sequence<lexer::rule::Literal<';'>,
                                       lexer::rule::Star<lexer::rule::Any<1>>> {
  static constexpr auto printable_name = "<comment>";
  std::string_view content;
};

struct TokenAction {
  static void apply(char *begin, char *end, Requirement &r) noexcept {
    r.name = {begin, static_cast<size_t>(end - begin)};
  }
  static void apply(char *begin, char *end, Name &n) noexcept {
    n.name = {begin, static_cast<size_t>(end - begin)};
  }

  static void apply(char *begin, char *end, Variable &v) noexcept {
    v.name = {begin, static_cast<size_t>(end - begin)};
  }

  static void apply(char *begin, char *end, Number &n) noexcept {
    n.value = 0;
    int factor = 1;
    --end;
    for (; end != begin; --end, factor *= 10) {
      assert(LiteralClass::digit(*end));
      n.value += (*end - '0') * factor;
    }
    if (*end == '-') {
      n.value *= -1;
    } else if (*end != '+') {
      assert(LiteralClass::digit(*end));
      n.value += (*end - '0') * factor;
    }
  }

  static void apply(char *begin, char *end, Comment &c) noexcept {
    c.content = {begin, static_cast<size_t>(end - begin)};
  }
};

struct TokenSet
    : lexer::TokenSet<LParen, RParen, Hyphen, Equality, And, Or, Not, Define,
                      Domain, Problem, Increase, Decrease, Metric, Requirements,
                      Types, Constants, Predicates, Functions, Action,
                      Parameters, Precondition, Effect, DomainRef, Objects,
                      Init, Goal, Requirement, Variable, Name, Number,
                      Comment> {};

} // namespace token

using TokenAction = token::TokenAction;
using TokenSet = token::TokenSet;

} // namespace pddl

#endif /* end of include guard: TOKENS_HPP */
