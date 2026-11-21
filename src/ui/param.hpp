#pragma once

#include <climits>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "ui/types.hpp"

namespace ui {

class Font;

enum class ParamType : uint8_t { Int, Bool, Text, Color, Align, HAlign, VAlign, Font };

// A single component parameter. Components are configured the same way on the
// device and in tests: a bag of named values that is validated against a
// schema declared by the component class.
class Param {
public:
  using Value = std::variant<std::monostate, int, bool, std::string, Color,
                             Align, HAlign, VAlign, const Font *>;

  Param() = default;
  Param(int value) : value_(value) {}
  Param(bool value) : value_(value) {}
  Param(const char *value) : value_(std::string(value)) {}
  Param(std::string value) : value_(std::move(value)) {}
  Param(Color value) : value_(value) {}
  Param(Align value) : value_(value) {}
  Param(HAlign value) : value_(value) {}
  Param(VAlign value) : value_(value) {}
  Param(const Font *value) : value_(value) {}

  bool empty() const { return std::holds_alternative<std::monostate>(value_); }
  ParamType type() const;
  bool is(ParamType type) const { return this->type() == type; }

  int as_int(int fallback = 0) const;
  bool as_bool(bool fallback = false) const;
  std::string as_text(const std::string &fallback = "") const;
  Color as_color(Color fallback = Color::Black) const;
  Align as_align(Align fallback = Align::TopLeft) const;
  HAlign as_halign(HAlign fallback = HAlign::Left) const;
  VAlign as_valign(VAlign fallback = VAlign::Top) const;
  const Font *as_font() const;

  // Human-readable form used by logs and test failures, e.g. int(12),
  // text("Success"), color(white).
  std::string dump() const;

  bool operator==(const Param &other) const { return value_ == other.value_; }
  bool operator!=(const Param &other) const { return !(*this == other); }

private:
  Value value_;
};

class ParamSet {
public:
  ParamSet() = default;
  ParamSet(std::initializer_list<std::pair<std::string, Param>> items);

  ParamSet &set(std::string key, Param value);
  ParamSet &remove(const std::string &key);

  const Param *find(const std::string &key) const;
  bool has(const std::string &key) const;
  int get_int(const std::string &key, int fallback = 0) const;
  bool get_bool(const std::string &key, bool fallback = false) const;
  std::string get_text(const std::string &key,
                       const std::string &fallback = "") const;
  Color get_color(const std::string &key, Color fallback = Color::Black) const;
  Align get_align(const std::string &key, Align fallback = Align::TopLeft) const;
  HAlign get_halign(const std::string &key, HAlign fallback = HAlign::Left) const;
  VAlign get_valign(const std::string &key, VAlign fallback = VAlign::Top) const;
  const Font *get_font(const std::string &key) const;

  size_t size() const { return items_.size(); }
  const std::string &key_at(size_t index) const { return items_[index].first; }
  const Param &value_at(size_t index) const { return items_[index].second; }

  std::string dump() const;

private:
  std::vector<std::pair<std::string, Param>> items_;
};

// Declares one accepted parameter. A spec with `required == false` and an
// empty default simply allows the key to be absent.
struct ParamSpec {
  std::string name;
  ParamType type;
  bool required = false;
  Param default_value;
  int min = INT_MIN;
  int max = INT_MAX;
};

struct ParamError {
  std::string key;
  std::string message;

  bool operator==(const ParamError &o) const {
    return key == o.key && message == o.message;
  }
};

// Validates `params` against `specs`: unknown keys, missing required keys,
// wrong value types and out-of-range ints are reported. The order of the
// returned errors follows `params`, so tests can assert on it.
std::vector<ParamError> validate_params(const ParamSet &params,
                                        const std::vector<ParamSpec> &specs);

const char *to_string(ParamType type);

} // namespace ui
