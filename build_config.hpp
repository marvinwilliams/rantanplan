#ifndef BUILD_CONFIG_HPP
#define BUILD_CONFIG_HPP

constexpr auto VERSION_MAJOR = 0;
constexpr auto VERSION_MINOR = 1;

#ifdef DEBUG_BUILD
static constexpr bool DEBUG_MODE = true;
#else
static constexpr bool DEBUG_MODE = false;
#endif

#endif /* end of include guard: BUILD_CONFIG_HPP */
