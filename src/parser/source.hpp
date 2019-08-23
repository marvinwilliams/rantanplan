#ifndef SOURCE_MANAGER_HPP
#define SOURCE_MANAGER_HPP

#include "lexer_exception.hpp"
#include "source_traits.hpp"
#include <string>
#include <vector>

namespace lexer {

template <typename CharT = char, typename Traits = SourceTraits<CharT>>
class Source {
public:
  using char_type = CharT;
  using source_position = size_t;

  template <typename Iterator>
  explicit Source(const std::string &name, Iterator begin,
                  const Iterator &end) {
    content_.reserve(content_.size() + std::distance(begin, end));
    content_.insert(content_.cend(), begin, end);
  }

  void reset_position() { current_position_ = 0; }

  void reset_sources() {
    content_.clear();
    current_position_ = 0;
  }

  bool is_past_input(source_position position) const {
    return position >= content_.size();
  }

  bool is_past_input() const { return is_past_input(current_position_); }

  void seek(source_position position) { current_position_ = position; }

  void advance(source_position count = 1) { current_position_ += count; }

  char_type get_char(source_position position) const {
    if (Traits::check_ranges && is_past_input(position)) {
      throw LexerException("Accessing position past input");
    }
    return content_[position];
  }

  char_type get_char() const { return get_char(current_position_); }

private:
  std::vector<char_type> content_;
  source_position current_position_;
};

} // namespace lexer

#endif /* end of include guard: SOURCE_MANAGER_HPP */
