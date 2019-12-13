#ifndef MODEL_EXCEPTION_HPP
#define MODEL_EXCEPTION_HPP

#include <exception>
#include <string>

namespace parsed {

class ModelException : public std::exception {
public:
  explicit ModelException(std::string message) noexcept
      : message_{std::move(message)} {}

  inline const char *what() const noexcept override { return message_.c_str(); }

private:
  std::string message_;
};

} // namespace parsed

#endif /* end of include guard: MODEL_EXCEPTION_HPP */
