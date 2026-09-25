// Pins ossia::fft for sizes that are not a power of two: the transform runs on
// the next power of two, and the samples past the requested count are zero,
// whatever the plan's input buffer held before (heap garbage, a previous
// execute() on input()).
#include <ossia/audio/fft.hpp>

#include <catch2/catch_all.hpp>

#include <bit>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
int nonFiniteBins(const ossia::fft_complex* out, std::size_t bins)
{
  int bad = 0;
  for(std::size_t k = 0; k < bins; ++k)
    if(!std::isfinite(out[k][0]) || !std::isfinite(out[k][1]))
      ++bad;
  return bad;
}
}

TEST_CASE("ossia::fft zero-pads a non-power-of-two input", "[ossia][fft]")
{
  const std::size_t count = GENERATE(441, 300, 1000);
  const std::size_t storage = std::bit_ceil(count);
  CAPTURE(count, storage);

  ossia::fft fft{count};
  std::vector<float> samples(count);
  for(std::size_t i = 0; i < count; ++i)
    samples[i] = std::sin(0.1 * double(i));

  for(std::size_t i = 0; i < storage; ++i)
    fft.input()[i] = std::numeric_limits<ossia::fft_real>::quiet_NaN();

  SECTION("full-length execute")
  {
    const auto* out = fft.execute(samples.data(), count);
    CHECK(nonFiniteBins(out, storage / 2 + 1) == 0);
    double dc = 0.;
    for(float s : samples)
      dc += s;
    CHECK(std::abs(out[0][0] - dc) < 1e-3);
  }

  SECTION("short execute")
  {
    const auto* out = fft.execute(samples.data(), count / 2);
    CHECK(nonFiniteBins(out, storage / 2 + 1) == 0);
  }
}
