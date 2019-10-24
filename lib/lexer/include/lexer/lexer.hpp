#ifndef LEXER_HPP
#define LEXER_HPP

#include "lexer/char_provider.hpp"
#include "lexer/lexer_exception.hpp"
#include "lexer/lexer_traits.hpp"
#include "lexer/literal_class.hpp"
#include "lexer/location.hpp"
#include "lexer/rules.hpp"

#include <cassert>
#include <algorithm>
#include <sstream>
#include <type_traits>
#include <variant>

namespace lexer {

namespace detail {

struct DefaultAction {};

template <typename, typename, typename = void>
struct has_action : std::false_type {};

template <typename Token, typename Action>
struct has_action<Token, Action,
                  std::void_t<decltype(Action::apply(std::declval<char *>(),
                                                     std::declval<char *>(),
                                                     std::declval<Token>()))>>
    : std::true_type {};

template <typename Token, typename Action>
constexpr inline bool has_action_v = has_action<Token, Action, void>::value;

} // namespace detail

template <typename RuleSet, typename Action = detail::DefaultAction,
          typename Traits = LexerTraits>
class Lexer {
public:
  using Token = typename RuleSet::Token;

  explicit Lexer(const std::string &name, char *begin, char *end)
      : name_{name}, location_{name_}, begin_{begin}, current_{begin},
        end_{end} {
    get_next_token();
  }

  explicit Lexer() noexcept : Lexer{"", nullptr, nullptr} {}

  void set_source(const std::string &name, char *begin, char *end) {
    name_ = name;
    location_ = Location{name_};
    begin_ = begin;
    current_ = begin;
    end_ = end;
    get_next_token();
  }

  std::string to_string() noexcept {
    return std::visit(
        [](const auto &t) { return std::string{t.printable_name}; }, token_);
  }

  template <typename TokenType> bool has_type() const noexcept {
    return std::holds_alternative<TokenType>(token_);
  }

  const Location &location() const noexcept { return location_; }

  bool at_end() const noexcept { return has_type<EndToken>(); }

  template <typename TokenType> const TokenType &get() const {
    return std::get<TokenType>(token_);
  }

  void next() { get_next_token(); }

private:
  void get_next_token() {
    // Skip blanks
    while (current_ != end_) {
      char current_char = *current_;
      if (LiteralClass::blank(current_char)) {
        location_.advance_column();
      } else if (LiteralClass::newline(current_char)) {
        location_.advance_line();
      } else {
        break;
      }
      ++current_;
    }
    location_.step();

    token_ = ErrorToken{};

    if (current_ == end_) {
      token_ = EndToken{};
      return;
    }

    char *token_end = end_;
    if (Traits::end_at_newline || Traits::end_at_blank) {
      token_end = std::find_if(current_, end_, [](char c) {
        return (Traits::end_at_newline && LiteralClass::newline(c)) ||
               (Traits::end_at_blank && LiteralClass::blank(c));
      });
    }

    CharProvider provider{current_, token_end};
    auto result = RuleSet::match(provider);
    auto next = current_ + (result.end - result.begin);

    std::visit(
        [this, next](auto &t) {
          if constexpr (detail::has_action_v<decltype(t), Action>) {
            Action::apply(current_, next, t);
          }
        },
        result.token);

    // If no rule matched and no rule accepts chars, emit an error
    if (result.begin == result.end) {
      assert(has_type<ErrorToken>());
      std::stringstream ss;
      ss << "Could not match token: \'" << *current_ << '\'';

      current_ = end_;

      // Location is one off because of early exit
      throw LexerException(location_ + 1, std::move(ss).str());
    }
    token_ = std::move(result.token);
    while (current_ != next) {
      location_.advance_column();
      if (LiteralClass::newline(*current_)) {
        location_.advance_line();
      }
      ++current_;
    }
  }

  Token token_ = ErrorToken{};
  std::string name_;
  Location location_;
  char *begin_ = nullptr;
  char *current_ = nullptr;
  char *end_ = nullptr;
};

} // namespace lexer

#endif /* end of include guard: LEXER_HPP */
