#include "ui/param.hpp"

#include <cstdio>

#include "ui/font.hpp"

namespace ui {
namespace {

const char *color_name(Color color) {
  switch (color) {
  case Color::Black:
    return "black";
  case Color::White:
    return "white";
  case Color::Invert:
    return "invert";
  }
  return "black";
}

} // namespace

ParamType Param::type() const {
  if (std::holds_alternative<std::monostate>(value_)) {
    return ParamType::Unset;
  }
  if (std::holds_alternative<int>(value_)) {
    return ParamType::Int;
  }
  if (std::holds_alternative<bool>(value_)) {
    return ParamType::Bool;
  }
  if (std::holds_alternative<std::string>(value_)) {
    return ParamType::Text;
  }
  if (std::holds_alternative<Color>(value_)) {
    return ParamType::Color;
  }
  if (std::holds_alternative<Align>(value_)) {
    return ParamType::Align;
  }
  if (std::holds_alternative<HAlign>(value_)) {
    return ParamType::HAlign;
  }
  if (std::holds_alternative<VAlign>(value_)) {
    return ParamType::VAlign;
  }
  if (std::holds_alternative<IconKind>(value_)) {
    return ParamType::Icon;
  }
  return ParamType::Font;
}

int Param::as_int(int fallback) const {
  const int *p = std::get_if<int>(&value_);
  return p ? *p : fallback;
}

bool Param::as_bool(bool fallback) const {
  const bool *p = std::get_if<bool>(&value_);
  return p ? *p : fallback;
}

std::string Param::as_text(const std::string &fallback) const {
  const std::string *p = std::get_if<std::string>(&value_);
  return p ? *p : fallback;
}

Color Param::as_color(Color fallback) const {
  const Color *p = std::get_if<Color>(&value_);
  return p ? *p : fallback;
}

Align Param::as_align(Align fallback) const {
  const Align *p = std::get_if<Align>(&value_);
  return p ? *p : fallback;
}

HAlign Param::as_halign(HAlign fallback) const {
  const HAlign *p = std::get_if<HAlign>(&value_);
  return p ? *p : fallback;
}

VAlign Param::as_valign(VAlign fallback) const {
  const VAlign *p = std::get_if<VAlign>(&value_);
  return p ? *p : fallback;
}

IconKind Param::as_icon(IconKind fallback) const {
  const IconKind *p = std::get_if<IconKind>(&value_);
  return p ? *p : fallback;
}

const Font *Param::as_font() const {
  const Font *const *p = std::get_if<const Font *>(&value_);
  return p ? *p : nullptr;
}

std::string Param::dump() const {
  char buf[96];
  switch (type()) {
  case ParamType::Unset:
    return "unset";
  case ParamType::Int:
    std::snprintf(buf, sizeof(buf), "int(%d)", as_int());
    return buf;
  case ParamType::Bool:
    return as_bool() ? "bool(true)" : "bool(false)";
  case ParamType::Text:
    return "text(\"" + as_text() + "\")";
  case ParamType::Color:
    return std::string("color(") + color_name(as_color()) + ")";
  case ParamType::Align:
    return std::string("align(") + to_string(as_align()) + ")";
  case ParamType::HAlign:
    return std::string("halign(") + to_string(as_halign()) + ")";
  case ParamType::VAlign:
    return std::string("valign(") + to_string(as_valign()) + ")";
  case ParamType::Icon:
    return std::string("icon(") + to_string(as_icon()) + ")";
  case ParamType::Font:
    return std::string("font(") + (as_font() ? as_font()->name : "null") + ")";
  }
  return "unset";
}

ParamSet::ParamSet(std::initializer_list<std::pair<std::string, Param>> items) {
  for (const auto &item : items) {
    set(item.first, item.second);
  }
}

ParamSet &ParamSet::set(std::string key, Param value) {
  for (auto &item : items_) {
    if (item.first == key) {
      item.second = std::move(value);
      return *this;
    }
  }
  items_.emplace_back(std::move(key), std::move(value));
  return *this;
}

ParamSet &ParamSet::remove(const std::string &key) {
  for (auto it = items_.begin(); it != items_.end(); ++it) {
    if (it->first == key) {
      items_.erase(it);
      return *this;
    }
  }
  return *this;
}

const Param *ParamSet::find(const std::string &key) const {
  for (const auto &item : items_) {
    if (item.first == key) {
      return &item.second;
    }
  }
  return nullptr;
}

bool ParamSet::has(const std::string &key) const { return find(key) != nullptr; }

int ParamSet::get_int(const std::string &key, int fallback) const {
  const Param *p = find(key);
  return p ? p->as_int(fallback) : fallback;
}

bool ParamSet::get_bool(const std::string &key, bool fallback) const {
  const Param *p = find(key);
  return p ? p->as_bool(fallback) : fallback;
}

std::string ParamSet::get_text(const std::string &key,
                               const std::string &fallback) const {
  const Param *p = find(key);
  return p ? p->as_text(fallback) : fallback;
}

Color ParamSet::get_color(const std::string &key, Color fallback) const {
  const Param *p = find(key);
  return p ? p->as_color(fallback) : fallback;
}

Align ParamSet::get_align(const std::string &key, Align fallback) const {
  const Param *p = find(key);
  return p ? p->as_align(fallback) : fallback;
}

HAlign ParamSet::get_halign(const std::string &key, HAlign fallback) const {
  const Param *p = find(key);
  return p ? p->as_halign(fallback) : fallback;
}

VAlign ParamSet::get_valign(const std::string &key, VAlign fallback) const {
  const Param *p = find(key);
  return p ? p->as_valign(fallback) : fallback;
}

IconKind ParamSet::get_icon(const std::string &key, IconKind fallback) const {
  const Param *p = find(key);
  return p ? p->as_icon(fallback) : fallback;
}

const Font *ParamSet::get_font(const std::string &key) const {
  const Param *p = find(key);
  return p ? p->as_font() : nullptr;
}

std::string ParamSet::dump() const {
  std::string out = "{";
  for (size_t i = 0; i < items_.size(); ++i) {
    if (i) {
      out += ", ";
    }
    out += items_[i].first;
    out += "=";
    out += items_[i].second.dump();
  }
  out += "}";
  return out;
}

const char *to_string(ParamType type) {
  switch (type) {
  case ParamType::Unset:
    return "unset";
  case ParamType::Int:
    return "int";
  case ParamType::Bool:
    return "bool";
  case ParamType::Text:
    return "text";
  case ParamType::Color:
    return "color";
  case ParamType::Align:
    return "align";
  case ParamType::HAlign:
    return "halign";
  case ParamType::VAlign:
    return "valign";
  case ParamType::Icon:
    return "icon";
  case ParamType::Font:
    return "font";
  }
  return "int";
}

std::vector<ParamError> validate_params(const ParamSet &params,
                                        const std::vector<ParamSpec> &specs) {
  std::vector<ParamError> errors;

  for (size_t i = 0; i < params.size(); ++i) {
    const std::string &key = params.key_at(i);
    const Param &value = params.value_at(i);

    const ParamSpec *spec = nullptr;
    for (const auto &candidate : specs) {
      if (candidate.name == key) {
        spec = &candidate;
        break;
      }
    }

    if (spec == nullptr) {
      errors.push_back({key, "unknown parameter"});
      continue;
    }
    if (value.empty()) {
      continue; // unset value: treated as absent
    }
    if (!value.is(spec->type)) {
      errors.push_back({key, std::string("expected ") + to_string(spec->type) +
                                 ", got " + to_string(value.type())});
      continue;
    }
    if (spec->type == ParamType::Int) {
      const int v = value.as_int();
      if (v < spec->min || v > spec->max) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "out of range: %d not in [%d, %d]", v,
                      spec->min, spec->max);
        errors.push_back({key, buf});
      }
    }
  }

  for (const auto &spec : specs) {
    if (!spec.required) {
      continue;
    }
    const Param *value = params.find(spec.name);
    if (value == nullptr || value->empty()) {
      errors.push_back({spec.name, "missing required parameter"});
    }
  }

  return errors;
}

} // namespace ui
