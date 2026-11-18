#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "modules/waveform_data.hpp"

using namespace waybar::modules::waveform;

TEST_CASE("Waveform pills aggregate recent measured history without synthetic motion",
          "[waveform]") {
  REQUIRE(pill_levels({}) == std::array<double, 7>{});
  REQUIRE(pill_levels(std::vector<double>(128, 0)) == std::array<double, 7>{});
  REQUIRE(pill_levels(std::vector<double>(128, 1)) == std::array<double, 7>{1, 1, 1, 1, 1, 1, 1});
  std::vector<double> history(100, 1);
  for (unsigned bin = 0; bin < 7; ++bin)
    for (unsigned i = 0; i < 4; ++i) history.push_back(bin / 6.0);
  auto values = pill_levels(history);
  for (unsigned bin = 0; bin < 7; ++bin) REQUIRE(std::abs(values[bin] - bin / 6.0) < 1e-12);
  history.assign(28, 0);
  for (unsigned i = 3; i < 28; i += 4) history[i] = 1;
  REQUIRE(pill_levels(history) == std::array<double, 7>{.5, .5, .5, .5, .5, .5, .5});
}

TEST_CASE("Waveform bounds configured animation frame rate", "[waveform]") {
  REQUIRE(frames_per_second(Json::Value()) == 60);
  REQUIRE(frames_per_second(Json::Value("invalid")) == 60);
  REQUIRE(frames_per_second(Json::Value(120)) == 60);
  REQUIRE(frames_per_second(Json::Value(30)) == 30);
  REQUIRE(frames_per_second(Json::Value(0)) == 1);
  REQUIRE(frames_per_second(Json::Value(-60)) == 1);
}

TEST_CASE("Waveform validates measured history and metadata", "[waveform]") {
  auto frame = parse(
      R"({"levels":[-1,0.5,2],"capturing":true,"state":"listening","sequence":18446744073709551615,"sample_interval_ms":1,"tooltip":"<plain>"})");
  REQUIRE(frame);
  REQUIRE(frame->levels == std::vector<double>{0, .5, 1});
  REQUIRE(frame->sequence == Json::UInt64(-1));
  REQUIRE(frame->interval_ms == 16);
  REQUIRE(frame->tooltip == "<plain>");
  REQUIRE_FALSE(parse("null"));
  REQUIRE_FALSE(parse("{} {}"));
  REQUIRE_FALSE(parse(std::string(32, '[') + "0" + std::string(32, ']')));
  REQUIRE_FALSE(parse(R"({"levels":["bad"],"capturing":true,"state":"listening"})"));
  REQUIRE_FALSE(parse(R"({"levels":[],"capturing":"yes","state":"listening"})"));
  REQUIRE_FALSE(parse(std::string(max_line + 1, ' ')));
  Json::Value many;
  many["capturing"] = true;
  many["state"] = "listening";
  for (unsigned i = 0; i < 129; ++i) many["levels"].append(.5);
  REQUIRE_FALSE(parse(Json::writeString(Json::StreamWriterBuilder(), many)));
}

TEST_CASE("Waveform transport bounds records and recovers after overflow", "[waveform]") {
  Lines lines;
  std::vector<std::string> records;
  auto consume = [&](const std::string& line) { records.push_back(line); };
  lines.feed("ab", 2, consume);
  lines.feed("c\n", 2, consume);
  REQUIRE(records == std::vector<std::string>{"abc"});
  std::string oversized(max_line + 1, 'x');
  lines.feed(oversized.data(), oversized.size(), consume);
  lines.feed("bad\nok\n", 7, consume);
  REQUIRE(records == std::vector<std::string>{"abc", "ok"});
  std::string boundary(max_line, 'x');
  boundary += '\n';
  lines.feed(boundary.data(), boundary.size(), consume);
  REQUIRE(records.back().size() == max_line);
}
