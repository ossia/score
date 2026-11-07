#include <Process/Dataflow/Wavetable.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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
          Values{Values{-1.f, 1.1f}}, Values{Values{false, 1.f}},
          Values{Values{std::numeric_limits<float>::infinity(), 1.f}}})
    CHECK_FALSE(Process::wavetableFromValue(invalid));

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
