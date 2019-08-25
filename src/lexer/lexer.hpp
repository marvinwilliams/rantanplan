#ifndef LEXER_HPP
#define LEXER_HPP

#include "lexer/lexer_exception.hpp"
#include "lexer/lexer_traits.hpp"
#include "lexer/location.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace lexer {

// Used for default initialization and represents an invalid token.
struct ErrorToken {};

template <typename Matcher, typename CharT = char,
          typename Traits = LexerTraits<CharT>>
class Lexer {
public:
  using char_type = CharT;
  using Token = typename Matcher::Token;

  // This iterator is used to extract the tokens. New tokens will be read when
  // incrementing the iterator.
  template <typename CharIterator> class TokenIterator {
  public:
    using iterator_category = typename CharIterator::iterator_category;
    using value_type = Token;
    using difference_type = std::ptrdiff_t;
    using pointer = const Token *;
    using reference = const Token &;

    friend class Lexer;

    explicit TokenIterator(const Lexer *lexer, const CharIterator &from,
                           const CharIterator &to, const CharIterator &end,
                           const value_type &token, const Location &location)
        : lexer_{lexer}, from_{from}, to_{to}, end_{end}, token_{token},
          location_{location} {}

    TokenIterator &operator++() {
      lexer_->get_next_token(*this);
      return *this;
    }

    TokenIterator operator++(int) {
      TokenIterator old = *this;
      lexer_->get_next_token(*this);
      return old;
    }

    bool operator==(const TokenIterator &other) const {
      return lexer_ == other.lexer_ && from_ == other.from_ &&
             to_ == other.to_ && end_ == other.end_;
    }

    bool operator!=(const TokenIterator &other) const {
      return !(*this == other);
    }

    reference token() const { return token_; }
    const Location &location() const { return location_; }
    reference operator*() const { return token(); }
    bool end() const { return from_ == end_; }

  private:
    const Lexer *lexer_;
    CharIterator from_;
    CharIterator to_;
    const CharIterator end_;
    value_type token_;
    Location location_;
  };

  template <typename CharIterator>
  TokenIterator<CharIterator> lex(const std::string &name, CharIterator begin,
                                  CharIterator end) {
    TokenIterator<CharIterator> iterator{this, begin,        begin,
                                         end,  ErrorToken{}, Location{name}};
    get_next_token(iterator);
    return iterator;
  }

private:
  template <typename CharIterator>
  void get_next_token(TokenIterator<CharIterator> &token_iterator) const {
    // Skip blanks
    while (token_iterator.to_ != token_iterator.end_) {
      CharT current_char = *token_iterator.to_;
      if (Traits::is_blank(current_char)) {
        token_iterator.location_.advance_column();
      } else if (Traits::is_newline(current_char)) {
        token_iterator.location_.advance_line();
      } else {
        break;
      }
      token_iterator.to_++;
    }
    token_iterator.from_ = token_iterator.to_;
    token_iterator.location_.step();

    // Check if end is reached
    if (token_iterator.end()) {
      token_iterator.token_ = ErrorToken{};
      return;
    }

    std::vector<CharT> current_string;
    Matcher matcher;

    // Read the next char until it cannot be part of any token
    while (token_iterator.to_ != token_iterator.end_) {
      CharT current_char = *token_iterator.to_;
      if (Traits::end_at_newline && Traits::is_newline(current_char)) {
        matcher.end();
        break;
      }
      if (Traits::end_at_blank && Traits::is_blank(current_char)) {
        matcher.end();
        break;
      }
      matcher.match(current_string, current_char);
      if (!matcher.valid()) {
        break;
      }
      current_string.push_back(current_char);
      token_iterator.location_.advance_column();
      if (!Traits::end_at_newline && Traits::is_newline(current_char)) {
        token_iterator.location_.advance_line();
      }
      token_iterator.to_++;
    }
    // The char cannot be part of any token
    if (current_string.empty()) {
      // Adjust location because of early exit
      CharT error_char = *token_iterator.to_;
      token_iterator.to_++;
      token_iterator.location_.advance_column();
      token_iterator.token_ = ErrorToken{};
      std::stringstream msg;
      msg << "Unexpected character: \'" << error_char << '\'';
      throw LexerException(token_iterator.location_, msg.str());
    }
    // In case there was no char left we need to update the matchers' status
    if (token_iterator.to_ == token_iterator.end_) {
      matcher.end();
    }
    token_iterator.token_ = matcher.get_token(current_string);
  }
};

} // namespace lexer

#endif /* end of include guard: LEXER_HPP */
