#ifndef RULES_HPP
#define RULES_HPP

#include "lexer/char_provider.hpp"
#include "lexer/literal_class.hpp"
#include "lexer/rule_matcher.hpp"
#include "lexer/token.hpp"

#include <string>
#include <type_traits>

namespace lexer {

namespace rule {

namespace detail {

constexpr char to_lower(char c) {
  if (LiteralClass::upper(c)) {
    return static_cast<char>(c - ('A' - 'a'));
  }
  return c;
}

constexpr char to_upper(char c) {
  if (LiteralClass::lower(c)) {
    return static_cast<char>(c + ('A' - 'a'));
  }
  return c;
}

} // namespace detail

struct basic_rule : basic_token {};

struct Empty : basic_rule {
  static constexpr bool match(CharProvider &) noexcept { return true; }

  static constexpr auto printable_name = "<empty>";
};

template <char c> struct Literal : basic_rule {
  static constexpr bool match(CharProvider &provider) noexcept {
    if (provider.length() > 0 && provider.get() == c) {
      provider.bump();
      return true;
    }
    provider.reset();
    return false;
  }

  static constexpr auto printable_name = "<literal>";
};

template <char c> struct ILiteral : basic_rule {
  static bool match(CharProvider &provider) noexcept {
    if (provider.length() > 0 &&
        detail::to_lower(provider.get()) == detail::to_lower(c)) {
      provider.bump();
      return true;
    }
    provider.reset();
    return false;
  }

  static constexpr auto printable_name = "<ci_literal>";
};

struct Whitespace : basic_rule {
  static bool match(CharProvider &provider) noexcept {
    if (provider.length() > 0 && LiteralClass::blank(provider.get())) {
      provider.bump();
      return true;
    }
    provider.reset();
    return false;
  }

  static constexpr auto printable_name = "<whitespace>";
};

struct Digit : basic_rule {
  static bool match(CharProvider &provider) noexcept {
    if (provider.length() > 0 && LiteralClass::digit(provider.get())) {
      provider.bump();
      return true;
    }
    provider.reset();
    return false;
  }

  static constexpr auto printable_name = "<digit>";
};

struct UpperCase : basic_rule {
  static bool match(CharProvider &provider) noexcept {
    if (provider.length() > 0 && LiteralClass::upper(provider.get())) {
      provider.bump();
      return true;
    }
    provider.reset();
    return false;
  }

  static constexpr auto printable_name = "<uppercase>";
};

struct LowerCase : basic_rule {
  static bool match(CharProvider &provider) noexcept {
    if (provider.length() > 0 && LiteralClass::lower(provider.get())) {
      provider.bump();
      return true;
    }
    provider.reset();
    return false;
  }

  static constexpr auto printable_name = "<lowercase>";
};

struct Alpha : basic_rule {
  static bool match(CharProvider &provider) noexcept {
    if (provider.length() > 0 && LiteralClass::alpha(provider.get())) {
      provider.bump();
      return true;
    }
    provider.reset();
    return false;
  }

  static constexpr auto printable_name = "<alpha>";
};

struct Alnum : basic_rule {
  static bool match(CharProvider &provider) noexcept {
    if (provider.length() > 0 && LiteralClass::alnum(provider.get())) {
      provider.bump();
      return true;
    }
    provider.reset();
    return false;
  }

  static constexpr auto printable_name = "<alnum>";
};

template <typename Predicate> struct LiteralIf : basic_rule {
  static bool match(CharProvider &provider) noexcept {
    if (provider.length() > 0 && Predicate{}(provider.get())) {
      provider.bump();
      return true;
    }
    provider.reset();
    return false;
  }

  static constexpr auto printable_name = "<literalif>";
};

template <char... cs> struct Word : basic_rule {
  static bool match(CharProvider &provider) noexcept {
    if (provider.length() >= sizeof...(cs)) {
      if (((provider.get() == cs) && ...)) {
        provider.bump();
        return true;
      }
    }
    provider.reset();
    return false;
  }

  static constexpr auto printable_name = "<word>";
};

template <char... cs> struct IWord : basic_rule {
  static bool match(CharProvider &provider) noexcept {
    if (provider.length() >= sizeof...(cs)) {
      if (((detail::to_lower(provider.get()) == detail::to_lower(cs)) && ...)) {
        provider.bump();
        return true;
      }
    }
    provider.reset();
    return false;
  }

  static constexpr auto printable_name = "<ci_word>";
};

template <size_t N> struct Any : basic_rule {
  static bool match(CharProvider &provider) noexcept {
    if (provider.length() < N) {
      return false;
    }
    provider.skip(N);
    provider.bump();
    return true;
  }

  static constexpr auto printable_name = "<any>";
};

template <typename Rule> struct Optional : basic_rule {
  static bool match(CharProvider &provider) noexcept {
    Rule::match(provider);
    return true;
  }
  static constexpr auto printable_name = "<optional>";
};

template <typename... Rules> struct Sequence : basic_rule {
  static bool match(CharProvider &provider) noexcept {
    auto current = provider.get_pos();
    bool result = (Rules::match(provider) && ...);
    if (!result) {
      provider.set_pos(current);
    }
    return result;
  }

  static constexpr auto printable_name = "<sequence>";
};

template <typename... Rules> struct Choice : basic_rule {
  static bool match(CharProvider &provider) noexcept {
    return (Rules::match(provider) || ...);
  }

  static constexpr auto printable_name = "<choice>";
};

template <typename Rule> struct Star : basic_rule {
  static bool match(CharProvider &provider) noexcept {
    while (Rule::match(provider)) {
    }
    return true;
  }

  static constexpr auto printable_name = "<star>";
};

template <typename Rule> struct Plus : Sequence<Rule, Star<Rule>> {
  static constexpr auto printable_name = "<plus>";
};

template <typename Rule> struct And : basic_rule {
  static bool match(CharProvider &provider) noexcept {
    auto current = provider.get_pos();
    bool result = Rule::match(provider);
    provider.set_pos(current);
    return result;
  }

  static constexpr auto printable_name = "<And>";
};

template <typename Rule> struct Not : basic_rule {
  static bool match(CharProvider &provider) noexcept {
    auto current = provider.get_pos();
    bool result = Rule::match(provider);
    provider.set_pos(current);
    return !result;
  }

  static constexpr auto printable_name = "<Not>";
};

} // namespace rule

} // namespace lexer

#endif /* end of include guard: RULES_HPP */
