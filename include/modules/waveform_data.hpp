#pragma once

#include <json/json.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace waybar::modules::waveform {
constexpr size_t max_line = 16384;
constexpr size_t max_samples = 128;

struct Frame {
  std::vector<double> levels;
  bool capturing = false;
  std::string state = "off", css_class, tooltip;
  Json::UInt64 sequence = 0;
  unsigned interval_ms = 50;
};

inline std::optional<Frame> parse(const std::string& line) {
  if (line.size() > max_line) return std::nullopt;
  Json::CharReaderBuilder builder;
  builder["collectComments"] = false;
  builder["stackLimit"] = 16;
  builder["failIfExtra"] = true;
  builder["rejectDupKeys"] = true;
  Json::Value value;
  std::string errors;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  try {
    if (!reader->parse(line.data(), line.data() + line.size(), &value, &errors))
      return std::nullopt;
  } catch (const Json::Exception&) {
    return std::nullopt;
  }
  if (!value.isObject() || !value["levels"].isArray() || value["levels"].size() > max_samples ||
      !value["capturing"].isBool() || !value["state"].isString())
    return std::nullopt;
  Frame frame;
  frame.capturing = value["capturing"].asBool();
  frame.state = value["state"].asString();
  for (const auto& level : value["levels"]) {
    if (!level.isNumeric() || !std::isfinite(level.asDouble())) return std::nullopt;
    frame.levels.push_back(std::clamp(level.asDouble(), 0.0, 1.0));
  }
  if (value["class"].isString()) frame.css_class = value["class"].asString();
  if (value["tooltip"].isString()) frame.tooltip = value["tooltip"].asString();
  if (value["sequence"].isUInt64()) frame.sequence = value["sequence"].asUInt64();
  if (value["sample_interval_ms"].isUInt())
    frame.interval_ms = std::clamp(value["sample_interval_ms"].asUInt(), 16u, 1000u);
  return frame;
}

// Overflow discards the entire record, never interpreting a truncated suffix as JSON.
class Lines {
 public:
  template <typename Consumer>
  void feed(const char* data, size_t size, Consumer consume) {
    for (size_t i = 0; i < size; ++i) {
      if (data[i] == '\n') {
        if (!overflow_) consume(line_);
        line_.clear();
        overflow_ = false;
      } else if (!overflow_) {
        if (line_.size() == max_line) {
          line_.clear();
          overflow_ = true;
        } else {
          line_ += data[i];
        }
      }
    }
  }

 private:
  std::string line_;
  bool overflow_ = false;
};
}  // namespace waybar::modules::waveform
