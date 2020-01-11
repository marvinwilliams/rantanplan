#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "logging/logging.hpp"

#include <algorithm>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <typeinfo>
#include <vector>

namespace options {

class OptionException : public std::exception {
public:
  explicit OptionException(std::string message) noexcept
      : message_{std::move(message)} {}

  inline const char *what() const noexcept override { return message_.c_str(); }

private:
  std::string message_;
};

struct OptionStateBase {
  virtual ~OptionStateBase() = default;

  unsigned int count = 0;
};

template <typename T> struct OptionState : OptionStateBase { T value; };

template <> struct OptionState<bool> : OptionStateBase {};

struct BaseOption {
  virtual ~BaseOption() = default;

  virtual void parse(std::string_view text) = 0;
  virtual const OptionStateBase &get() const = 0;
  virtual bool is_flag() { return false; }

  std::string name;
  char short_name;
  std::string description;

protected:
  explicit BaseOption(std::string name, char short_name,
                      std::string description)
      : name{std::move(name)}, short_name{short_name}, description{std::move(
                                                           description)} {
    if (this->name.length() == 0) {
      throw OptionException{"Name must not be empty"};
    }
    if (this->name[0] == '-') {
      throw OptionException{"Name must not start with '-'"};
    }
  }

  BaseOption(const BaseOption &) = default;
  BaseOption(BaseOption &&) = default;
  BaseOption &operator=(const BaseOption &) = default;
  BaseOption &operator=(BaseOption &&) = default;
};

template <typename T> struct DefaultParser {
  void operator()(std::string_view input, T &value) {
    std::stringstream ss;
    ss << input;
    ss >> value;
  }
};

inline bool t = true;

template <typename T, typename Parser = DefaultParser<T>>
class Option : public BaseOption {
public:
  explicit Option(std::string name, char short_name,
                  std::string description, Parser&& parser) : BaseOption{
            std::move(name),
            short_name,
            std::move(description),
        }, parser_{std::forward<Parser>(parser)} {}

  void parse(std::string_view input) override {
    ++state_.count;
    state_.value = parser_(input);
  }

  const OptionState<T> &get() const override { return state_; }

private:
  OptionState<T> state_;
  Parser parser_;
};

template <> class Option<bool> : public BaseOption {
public:
  explicit Option(std::string name, char short_name, std::string description)
      : BaseOption{std::move(name), short_name, std::move(description)} {}

  void parse(std::string_view) override { ++state_.count; }
  bool is_flag() override { return true; }

  const OptionState<bool> &get() const override { return state_; }

private:
  OptionState<bool> state_;
};

struct OptionName {
  std::string name;
  char short_name = '\0';
};

class Options {
public:
  explicit Options(const std::string &description) noexcept
      : description_{description} {}

  template <typename T, typename Parser = DefaultParser<T>,
            typename std::enable_if_t<!std::is_same<T, bool>::value, int> = 0>
  void add_option(OptionName name, std::string description,
                  Parser &&parser = Parser{}) {
    if (exists(name.name)) {
      throw OptionException{"Option \'" + name.name + "\' already exists"};
    }
    auto n = name.name;
    options_[n] = std::make_unique<Option<T, Parser>>(
        std::move(name.name), name.short_name, std::move(description),
        std::forward<Parser>(parser));
  }

  template <typename T, std::enable_if_t<std::is_same<T, bool>::value, int> = 0>
  void add_option(OptionName name, std::string description) {
    if (exists(name.name)) {
      throw OptionException{"Option \'" + name.name + "\' already exists"};
    }
    auto n = name.name;
    options_[n] = std::make_unique<Option<bool>>(
        std::move(name.name), name.short_name, std::move(description));
  }

  template <typename T, typename Parser = DefaultParser<T>,
            typename std::enable_if_t<!std::is_same<T, bool>::value, int> = 0>
  void add_positional_option(std::string name, std::string description,
                             Parser &&parser = Parser{}) {
    if (exists(name)) {
      throw OptionException{"Option \'" + name + "\' already exists"};
    }
    auto option = std::make_unique<Option<T, Parser>>(
        std::move(name), '\0', std::move(description),
        std::forward<Parser>(parser));
    positional_options_.push_back(std::move(option));
  }

  void parse(int argc, char *const argv[]) const {
    size_t counter = 0;
    size_t positional_counter = 0;

    while (counter < static_cast<size_t>(argc - 1)) {
      std::string_view arg{argv[counter + 1]};
      if (arg[0] != '-') {
        if (positional_counter < positional_options_.size()) {
          positional_options_[positional_counter]->parse(arg);
          ++positional_counter;
          ++counter;
          continue;
        } else {
          throw OptionException{"Unexpected positional argument \'" +
                                std::string{arg} + "\'"};
        }
      }

      arg.remove_prefix(1);

      if (arg.length() == 0) {
        throw OptionException{"Single \'-\' found"};
      }

      auto match = options_.cbegin();

      if (arg[0] == '-') {
        // Single option
        arg.remove_prefix(1);
        match = options_.find(std::string{arg});
        if (match == options_.cend()) {
          throw OptionException{"Could not match option \'--" +
                                std::string{arg} + "\'"};
        }
      } else {
        // Short options
        for (auto it = arg.cbegin(); it != arg.cend(); ++it) {
          match = std::find_if(options_.cbegin(), options_.cend(),
                               [&it](const auto &option) {
                                 if (option.second->short_name != '\0') {
                                   return option.second->short_name == *it;
                                 }
                                 return false;
                               });
          if (match == options_.cend()) {
            throw OptionException{"Could not match short option \'-" +
                                  std::string{*it} + "\'"};
          }
          if (it != arg.cend() - 1) {
            if (match->second->is_flag()) {
              match->second->parse("");
            } else {
              throw OptionException{"Option \'-" + std::string{*it} +
                                    "\' is not a flag"};
            }
          }
        }
      }
      if (match->second->is_flag()) {
        match->second->parse("");
      } else {
        ++counter;
        if (counter < static_cast<size_t>(argc - 1) &&
            argv[counter + 1][0] != '-') {
          match->second->parse(argv[counter + 1]);
        } else {
          throw OptionException{"Expected argument for option \'--" +
                                match->first + "\'"};
        }
      }
      ++counter;
    }
  }

  bool exists(std::string_view name) const {
    auto match = options_.find(std::string{name});
    if (match != options_.end()) {
      return true;
    }
    auto match_positional = std::find_if(
        positional_options_.cbegin(), positional_options_.cend(),
        [&name](const auto &option) { return option->name == name; });
    return match_positional != positional_options_.cend();
  }

  bool present(std::string_view name) const {
    return find_option_(name).get().count > 0;
  }

  template <typename T> const OptionState<T> &get(std::string_view name) const {
    const auto &option = find_option_(name);
    try {
      return dynamic_cast<const OptionState<T> &>(option.get());
    } catch (const std::bad_cast &) {
      throw OptionException{"Given type does not match option type"};
    }
  }

  void print_usage() const noexcept {
    std::cout << "Synopsis:\n\t" << description_ << ' ';
    for (auto it = positional_options_.cbegin();
         it != positional_options_.cend(); ++it) {
      std::cout << (*it)->name;
      std::cout << ' ';
    }
    if (options_.size() > 0) {
      std::cout << "[OPTION...]";
    }
    if (positional_options_.size() > 0) {
      std::cout << "\n\n";
      std::cout << "Positional arguments:\n\t";
    }
    for (auto it = positional_options_.cbegin();
         it != positional_options_.cend(); ++it) {
      if (it != positional_options_.cbegin()) {
        std::cout << "\n\t";
      }
      std::cout << (*it)->name << "\n\t\t" << (*it)->description;
    }
    if (options_.size() > 0) {
      std::cout << "\n\n";
      std::cout << "Options:\n\t";
    }
    for (auto it = options_.cbegin(); it != options_.cend(); ++it) {
      if (it != options_.cbegin()) {
        std::cout << "\n\t";
      }
      std::cout << "--" << it->second->name;
      if (it->second->short_name != '\0') {
        std::cout << ", -" << it->second->short_name;
      }
      std::cout << "\n\t\t" << it->second->description;
    }
    std::cout << std::endl;
  }

private:
  const BaseOption &find_option_(std::string_view name) const {
    auto match = options_.find(std::string{name});
    if (match != options_.end()) {
      return *match->second;
    }
    auto match_positional = std::find_if(
        positional_options_.cbegin(), positional_options_.cend(),
        [&name](const auto &option) { return option->name == name; });
    if (match_positional == positional_options_.cend()) {
      throw OptionException{"No option \'" + std::string{name} + "\'"};
    }
    return **match_positional;
  }

  std::string description_;
  std::unordered_map<std::string, std::unique_ptr<BaseOption>> options_;
  std::vector<std::unique_ptr<BaseOption>> positional_options_;
};

} // namespace options

#endif /* end of include guard: OPTIONS_HPP */
