// An FFT sized to the audio tick follows it when the audio server changes its
// block size. Planning allocates, so a size used before gets its plan back
// instead of a new one, and a size within the same power of two keeps it.

#include <ossia/audio/fft.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>

namespace
{
//! An impulse at frame 0: every bin has magnitude 1.
void check_impulse(ossia::fft& f)
{
  std::fill_n(f.input(), f.storage_size(), ossia::fft_real{});
  f.input()[0] = 1;
  auto* out = f.execute();
  for(std::size_t k = 0; k < f.storage_size() / 2 + 1; k++)
  {
    INFO("bin " << k << " of " << f.storage_size());
    REQUIRE(std::abs(std::hypot(out[k][0], out[k][1]) - 1.) < 1e-5);
  }
}
}

TEST_CASE("an FFT gets back the plan of a size it had", "[fft][buffer_size]")
{
  ossia::fft f{512};
  auto* const plan_512 = f.input();
  check_impulse(f);

  // Within the same power of two: the same plan
  f.reset(500);
  CHECK(f.size() == 500);
  CHECK(f.storage_size() == 512);
  CHECK(f.input() == plan_512);

  f.reset(1024);
  CHECK(f.storage_size() == 1024);
  auto* const plan_1024 = f.input();
  CHECK(plan_1024 != plan_512);
  check_impulse(f);

  // Back and forth: no new plan
  for(int i = 0; i < 4; i++)
  {
    f.reset(512);
    CHECK(f.input() == plan_512);
    check_impulse(f);
    f.reset(1024);
    CHECK(f.input() == plan_1024);
    check_impulse(f);
  }

  // More sizes than it keeps: the oldest goes, the others stay usable
  for(std::size_t n : {64, 128, 256, 2048, 4096})
  {
    f.reset(n);
    CHECK(f.storage_size() == n);
    check_impulse(f);
  }
  f.reset(2048);
  check_impulse(f);
}

TEST_CASE("an FFT prepared ahead plans nothing for nearby sizes", "[fft][buffer_size]")
{
  ossia::fft f{16};
  f.prepare(512);
  CHECK(f.size() == 512);
  check_impulse(f);
  auto* const p512 = f.input();

  // A quarter, half and double of it were planned too
  f.reset(128);
  auto* const p128 = f.input();
  check_impulse(f);
  f.reset(256);
  auto* const p256 = f.input();
  check_impulse(f);
  f.reset(1024);
  auto* const p1024 = f.input();
  check_impulse(f);

  for(int i = 0; i < 3; i++)
  {
    f.reset(512);
    CHECK(f.input() == p512);
    f.reset(256);
    CHECK(f.input() == p256);
    f.reset(1024);
    CHECK(f.input() == p1024);
  }

  // A new size replaces the plan unused for the longest: 128
  f.reset(512);
  f.reset(256);
  f.reset(2048);
  check_impulse(f);
  f.reset(256);
  CHECK(f.input() == p256);
  f.reset(512);
  CHECK(f.input() == p512);
  (void)p128;
}
