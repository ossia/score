#include <Process/Dataflow/Wavetable.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <functional>
#include <limits>

using Catch::Approx;

TEST_CASE("Wavetables preserve ordered frames at different resolutions", "[wavetable]")
{
  for(std::size_t resolution : {3, 32, 2048})
  {
    auto table = Process::makeTriangleWavetable(resolution);
    const auto first = table.samples;
    for(float sample : first)
      table.samples.push_back(-sample);
    REQUIRE(table.valid());
    const auto decoded = Process::wavetableFromValue(Process::wavetableToValue(table));
    REQUIRE(decoded);
    CHECK(*decoded == table);
    CHECK(decoded->frame(0).front() == -1.f);
    CHECK(decoded->frame(1).front() == 1.f);
    CHECK(decoded->frameSize == resolution);
    CHECK(decoded->frameCount() == 2);
  }
}

TEST_CASE("Periodic preparation wraps without a duplicated endpoint", "[wavetable]")
{
  const Process::WavetableData table{4, {-1.f, 0.f, 1.f, 0.f}};
  CHECK(Process::sampleWavetable(table, 0, 0.) == -1.f);
  CHECK(Process::sampleWavetable(table, 0, 1.) == -1.f);
  CHECK(Process::sampleWavetable(table, 0, -.125) == Approx(-.5));
  CHECK(Process::sampleWavetable(table, 0, .875) == Approx(-.5));
  std::array<unsigned char, 8> chip{};
  REQUIRE(Process::prepareWaveform(table, chip, 0., 15.));
  CHECK(chip == std::array<unsigned char, 8>{0, 4, 8, 11, 15, 11, 8, 4});
  std::array<float, 8> continuous{};
  REQUIRE(Process::prepareWaveform(table, continuous, -1., 1.));
  CHECK(continuous == std::array<float, 8>{-1.f, -.5f, 0.f, .5f, 1.f, .5f, 0.f, -.5f});
}

TEST_CASE("Malformed table content cannot partially replace prepared samples", "[wavetable]")
{
  const std::array<unsigned char, 4> original{1, 2, 3, 4};
  auto prepared = original;
  auto table = Process::makeTriangleWavetable(4);
  table.samples[2] = std::numeric_limits<float>::quiet_NaN();
  CHECK_FALSE(Process::prepareWaveform(table, prepared, 0., 15.));
  CHECK(prepared == original);
  CHECK_THROWS_AS(Process::wavetableToValue(table), std::invalid_argument);

  using Values = std::vector<ossia::value>;
  for(const auto& invalid : std::vector<ossia::value>{
          Values{}, Values{Values{0.f}}, Values{Values{-1.f, 1.f}, Values{0.f, 0.f, 0.f}},
          Values{Values{-1.f, 1.1f}}, Values{-1.f, 1.f}, Values{Values{std::string{"a"}, 1.f}},
          Values{Values{std::numeric_limits<float>::infinity(), 1.f}}})
    CHECK_FALSE(Process::wavetableFromValue(invalid));
  // The canonical form reads its numbers as every waveform value does.
  CHECK(Process::wavetableFromValue(Values{Values{false, 1}})
        == Process::WavetableData{2, {0.f, 1.f}});
  CHECK(Process::waveformNumber(ossia::value{true}) == 1.f);
  CHECK(Process::waveformNumber(ossia::value{-3}) == -3.f);
  CHECK(Process::waveformSample(ossia::value{-3}) == -1.f);
  CHECK_FALSE(Process::waveformNumber(ossia::value{std::string{"1"}}));
  CHECK_FALSE(Process::waveformNumber(ossia::value{std::numeric_limits<float>::quiet_NaN()}));

  table = Process::makeTriangleWavetable(4);
  CHECK_FALSE(Process::prepareWaveform(table, prepared, 15., 0.));
  CHECK_FALSE(Process::prepareWaveform(table, prepared, -1., 255.));
  CHECK_FALSE(Process::prepareWaveform(table, prepared, 0., 15., 1));
  CHECK(prepared == original);
}

TEST_CASE("Table limits bound dimensions before allocating content", "[wavetable]")
{
  using Values = std::vector<ossia::value>;
  CHECK_FALSE(Process::wavetableFromValue(
      Values(Process::WavetableData::maxFrames + 1, Values{-1.f, 1.f})));
  CHECK_FALSE(Process::wavetableFromValue(
      Values{Values(Process::WavetableData::maxFrameSize + 1, 0.f)}));
  CHECK_THROWS_AS(Process::makeTriangleWavetable(1), std::invalid_argument);
  CHECK_THROWS_AS(Process::makeTriangleWavetable(Process::WavetableData::maxFrameSize + 1),
                  std::invalid_argument);
  Process::WavetableData table{Process::WavetableData::maxFrameSize,
                             std::vector<float>(Process::WavetableData::maxSamples, 0.f)};
  REQUIRE(table.valid());
  table.samples.push_back(0.f);
  CHECK_FALSE(table.valid());
}

namespace
{
bool onLevels(std::span<const float> frame, int levels)
{
  return std::all_of(frame.begin(), frame.end(), [levels](float sample) {
    const double step = (sample + 1.) * .5 * (levels - 1);
    return sample >= -1.f && sample <= 1.f && std::abs(step - std::round(step)) < 1e-4;
  });
}
}

TEST_CASE("Quantization rounds to evenly spaced levels and reports their index", "[wavetable]")
{
  CHECK(Process::quantizeSample(.3, 0) == .3f);
  CHECK(Process::quantizeSample(4., 0) == 1.f);
  CHECK(Process::quantizeSample(std::numeric_limits<double>::quiet_NaN(), 0) == 0.f);
  CHECK(Process::quantizeSample(-1., 16) == -1.f);
  CHECK(Process::quantizeSample(1., 16) == 1.f);
  CHECK(Process::quantizeSample(.1, 3) == 0.f);
  CHECK(Process::quantizeSample(.6, 3) == 1.f);
  CHECK(Process::sampleLevel(-1.f, 32) == 0);
  CHECK(Process::sampleLevel(1.f, 32) == 31);
  CHECK(Process::sampleLevel(Process::quantizeSample(2. * 23 / 31 - 1., 32), 32) == 23);
}

TEST_CASE("Generated shapes fill the frame on the port's levels", "[wavetable]")
{
  using Shape = Process::WaveformShape;
  for(int levels : {0, 16, 256})
  {
    INFO("levels " << levels);
    std::vector<float> frame(32, 7.f);
    const auto generate = [&](Shape shape, std::uint32_t seed = 1) {
      Process::generateWaveform(frame, shape, levels, seed);
      if(levels)
        CHECK(onLevels(frame, levels));
      return frame;
    };
    const auto sine = generate(Shape::Sine);
    CHECK(sine[0] == Process::quantizeSample(0., levels));
    CHECK(sine[8] == 1.f);
    CHECK(sine[24] == -1.f);
    const auto triangle = generate(Shape::Triangle);
    if(levels == 0)
      CHECK(triangle == Process::makeTriangleWavetable(32).samples);
    CHECK(triangle[0] == -1.f);
    CHECK(triangle[16] == 1.f);
    const auto up = generate(Shape::SawUp);
    CHECK(up.front() == -1.f);
    CHECK(up.back() == 1.f);
    CHECK(std::is_sorted(up.begin(), up.end()));
    const auto down = generate(Shape::SawDown);
    for(std::size_t i = 0; i < 32; ++i)
      CHECK(down[i] == Approx(up[31 - i]).margin(1e-6));
    for(auto [shape, high] : {std::pair{Shape::Square, 16}, {Shape::Pulse25, 8}, {Shape::Pulse12, 4}})
    {
      const auto pulse = generate(shape);
      CHECK(std::count(pulse.begin(), pulse.end(), 1.f) == high);
      CHECK(std::count(pulse.begin(), pulse.end(), -1.f) == 32 - high);
      CHECK(std::all_of(pulse.begin(), pulse.begin() + high, [](float v) { return v == 1.f; }));
    }
    const auto noise = generate(Shape::Noise, 42);
    CHECK(generate(Shape::Noise, 42) == noise);
    CHECK(generate(Shape::Noise, 43) != noise);
    CHECK(std::all_of(noise.begin(), noise.end(), [](float v) { return v >= -1.f && v <= 1.f; }));
    CHECK(std::adjacent_find(noise.begin(), noise.end(), std::not_equal_to<>{}) != noise.end());
  }
}

TEST_CASE("Frame transforms keep the frame size and the port's levels", "[wavetable]")
{
  const std::vector<float> source{-.5f, 0.f, .25f, .5f, -.25f, 0.f};
  {
    auto frame = source;
    Process::normalizeWaveform(frame, 0);
    CHECK(frame == std::vector<float>{-1.f, 0.f, .5f, 1.f, -.5f, 0.f});
    std::vector<float> silent(4, 0.f);
    Process::normalizeWaveform(silent, 0);
    CHECK(silent == std::vector<float>(4, 0.f));
    auto quantized = std::vector<float>{-.2f, .1f, .3f, 0.f};
    Process::normalizeWaveform(quantized, 16);
    CHECK(onLevels(quantized, 16));
    CHECK(*std::max_element(quantized.begin(), quantized.end()) == 1.f);
  }
  {
    auto frame = source;
    Process::invertWaveform(frame, 0);
    CHECK(frame == std::vector<float>{.5f, -0.f, -.25f, -.5f, .25f, -0.f});
    std::vector<float> chip(16);
    Process::generateWaveform(chip, Process::WaveformShape::SawUp, 16);
    auto inverted = chip;
    Process::invertWaveform(inverted, 16);
    CHECK(onLevels(inverted, 16));
    for(std::size_t i = 0; i < chip.size(); ++i)
      CHECK(Process::sampleLevel(inverted[i], 16) == 15 - Process::sampleLevel(chip[i], 16));
  }
  {
    auto frame = source;
    Process::reverseWaveform(frame);
    CHECK(frame == std::vector<float>{0.f, -.25f, .5f, .25f, 0.f, -.5f});
  }
  {
    auto frame = source;
    Process::rotateWaveform(frame, true);
    CHECK(frame == std::vector<float>{0.f, .25f, .5f, -.25f, 0.f, -.5f});
    Process::rotateWaveform(frame, false);
    CHECK(frame == source);
    Process::rotateWaveform(frame, false);
    CHECK(frame == std::vector<float>{0.f, -.5f, 0.f, .25f, .5f, -.25f});
  }
  {
    std::vector<float> frame{1.f, -1.f, 1.f, -1.f};
    Process::smoothWaveform(frame, 0);
    CHECK(frame == std::vector<float>(4, 0.f));
    std::vector<float> spike{0.f, 0.f, 1.f, 0.f, 0.f};
    Process::smoothWaveform(spike, 0);
    CHECK(spike == std::vector<float>{0.f, .25f, .5f, .25f, 0.f});
    std::vector<float> chip{-1.f, 1.f, 1.f, -1.f, -1.f, 1.f};
    Process::smoothWaveform(chip, 16);
    CHECK(onLevels(chip, 16));
  }
  {
    auto frame = source;
    Process::clearWaveform(frame, 0);
    CHECK(frame == std::vector<float>(6, 0.f));
    Process::clearWaveform(frame, 16);
    CHECK(onLevels(frame, 16));
    CHECK(frame == std::vector<float>(6, Process::quantizeSample(0., 16)));
    CHECK(std::abs(frame[0]) <= 1.f / 15.f + 1e-6f);
  }
}

TEST_CASE("Resampling keeps every frame of a bank in order", "[wavetable]")
{
  auto bank = Process::makeTriangleWavetable(8);
  for(float sample : Process::makeTriangleWavetable(8).samples)
    bank.samples.push_back(-sample);
  const auto resized = Process::resampleWavetable(bank, 16);
  REQUIRE(resized.frameSize == 16);
  REQUIRE(resized.frameCount() == 2);
  REQUIRE(resized.valid());
  for(std::size_t frame = 0; frame < 2; ++frame)
    for(std::size_t sample = 0; sample < 16; ++sample)
      CHECK(resized.frame(frame)[sample]
            == Process::sampleWavetable(bank, frame, double(sample) / 16));
}
