#ifndef SOURCE_TRAITS_HPP
#define SOURCE_TRAITS_HPP

namespace lexer {

template <typename CharT> struct SourceTraits {
  static constexpr bool check_ranges = true;
};

} // namespace lexer

#endif /* end of include guard: SOURCE_TRAITS_HPP */
