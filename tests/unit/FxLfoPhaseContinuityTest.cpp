// LFO v3: the waveform at a given moment does not depend on how the timeline
// was cut into buffers, stays on the musical position when synced (tempo
// changes, playback started mid-bar), and the drift never repeats or jumps
// however long it runs.

#include <Fx/LFO_v3.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using Catch::Approx;
using namespace Nodes::LFO::v3;

namespace
{
constexpr double flicks_per_second = 705600000.;

Node make(Waveform w, float period, bool sync, float shape = 0.5f)
{
  Node lfo;
  lfo.inputs.period.value = period;
  lfo.inputs.period.sync = sync;
  lfo.inputs.shape.value = shape;
  lfo.inputs.ampl.value = 1.f;
  lfo.inputs.offset.value = 0.f;
  lfo.inputs.jitter.value = 0.f;
  lfo.inputs.phase.value = 0.f;
  lfo.inputs.waveform.value = w;
  return lfo;
}

float tick_free(Node& lfo, int64_t start, int64_t end)
{
  halp::tick_flicks tk{};
  tk.frames = 64;
  tk.start_in_flicks = start;
  tk.end_in_flicks = end;
  lfo.outputs.out.value.reset();
  lfo(tk);
  return lfo.outputs.out.value.value_or(NAN);
}

float tick_synced(Node& lfo, double q0, double q1, double tempo)
{
  halp::tick_flicks tk{};
  tk.frames = 64;
  tk.tempo = tempo;
  tk.signature = {4, 4};
  tk.start_position_in_quarters = q0;
  tk.end_position_in_quarters = q1;
  lfo.outputs.out.value.reset();
  lfo(tk);
  return lfo.outputs.out.value.value_or(NAN);
}
}

TEST_CASE("LFO v3: the value at a date does not depend on the buffer size", "[fx][lfo]")
{
  // Two runs through ticks of 7 ms and of 11 ms: every 77 ms a tick of each
  // starts at the same date, and reports the value there.
  const int64_t ms = int64_t(flicks_per_second / 1000);
  for(auto w : {Sin, Triangle, Saw, RampDown, Square, Drift})
  {
    auto a = make(w, 0.37f, false, 0.3f);
    auto b = make(w, 0.37f, false, 0.3f);
    b.seed = a.seed; // the same drift
    std::vector<float> va, vb;
    for(int i = 0; i < 7 * 11 * 40; i += 7)
    {
      const float v = tick_free(a, i * ms, (i + 7) * ms);
      if(i % 77 == 0)
        va.push_back(v);
    }
    for(int i = 0; i < 7 * 11 * 40; i += 11)
    {
      const float v = tick_free(b, i * ms, (i + 11) * ms);
      if(i % 77 == 0)
        vb.push_back(v);
    }
    REQUIRE(va.size() == vb.size());
    for(std::size_t k = 0; k < va.size(); k++)
    {
      INFO("waveform " << int(w) << " at " << k * 77 << " ms");
      CHECK(va[k] == Approx(vb[k]).margin(1e-6));
    }
  }
}

TEST_CASE("LFO v3: played backwards, the cycle retraces its steps", "[fx][lfo]")
{
  const int64_t dt = int64_t(flicks_per_second / 100);
  auto lfo = make(Sin, 1.f, false);
  std::vector<float> forward;
  for(int i = 0; i < 50; i++)
    forward.push_back(tick_free(lfo, i * dt, (i + 1) * dt));
  // Now from date 50 back to date 0: each tick starts where the forward one
  // ended.
  for(int i = 50; i > 1; i--)
  {
    const float v = tick_free(lfo, i * dt, (i - 1) * dt);
    CHECK(v == Approx(std::sin(2. * M_PI * i / 100.)).margin(1e-6));
  }
}

TEST_CASE("LFO v3: synced, the phase is the musical position", "[fx][lfo]")
{
  SECTION("playback starting mid-bar is still on the beats")
  {
    // A quarter-note cycle; playback starts at quarter 2.25.
    auto lfo = make(Sin, 60.f / 120.f, true);
    CHECK(tick_synced(lfo, 2.25, 2.5, 120.) == Approx(1.).margin(1e-5));
    CHECK(tick_synced(lfo, 2.5, 2.75, 120.) == Approx(0.).margin(1e-5));
    CHECK(tick_synced(lfo, 3., 3.25, 120.) == Approx(0.).margin(1e-5));
  }

  SECTION("through tempo changes, no drift and no jump")
  {
    // A bar-long cycle, the tempo sweeping from 60 to 200 BPM: after exactly
    // 64 bars the cycle is back at its start.
    double q = 0.;
    double tempo = 60.;
    auto lfo = make(Triangle, float(4. * 60. / tempo), true);
    float prev = tick_synced(lfo, 0., 0., tempo);
    float worst_step = 0.f;
    while(q < 256. - 1e-9)
    {
      tempo = 60. + 140. * q / 256.;
      // What the binding hands the object at this tempo
      lfo.inputs.period.value = float(4. * 60. / tempo);
      const double dq = std::min(256. - q, tempo / 60. * 512. / 48000.);
      const float v = tick_synced(lfo, q, q + dq, tempo);
      worst_step = std::max(worst_step, std::abs(v - prev));
      prev = v;
      q += dq;
    }
    // A 512-frame buffer is at most 0.036 quarter: the triangle moves by that
    CHECK(worst_step < 0.05f);
    lfo.inputs.period.value = float(4. * 60. / 200.);
    CHECK(tick_synced(lfo, 256., 256.01, 200.) == Approx(0.).margin(1e-4));
  }

  SECTION("retrigger restarts the cycle where it is pressed")
  {
    auto lfo = make(Sin, 60.f / 120.f, true);
    tick_synced(lfo, 0., 0.3, 120.);
    lfo.inputs.retrigger.value.emplace();
    CHECK(tick_synced(lfo, 0.3, 0.4, 120.) == Approx(0.).margin(1e-6));
    lfo.inputs.retrigger.value.reset();
    CHECK(tick_synced(lfo, 0.55, 0.6, 120.) == Approx(1.).margin(1e-5));
  }
}

TEST_CASE("LFO v3: the phase knob moves by up to half a cycle", "[fx][lfo]")
{
  auto lfo = make(Sin, 1.f, false);
  lfo.inputs.phase.value = 0.5f; // a quarter of a cycle
  CHECK(tick_free(lfo, 0, 1000) == Approx(1.).margin(1e-5));
  auto lfo2 = make(Sin, 1.f, false);
  lfo2.inputs.phase.value = 1.f; // half a cycle
  const auto quarter = int64_t(flicks_per_second / 4);
  CHECK(tick_free(lfo2, 0, quarter) == Approx(0.).margin(1e-5));
  CHECK(tick_free(lfo2, quarter, 2 * quarter) == Approx(-1.).margin(1e-5));
}

TEST_CASE("LFO v3: hours of drift, continuous and not repeating", "[fx][lfo]")
{
  // A 10 ms period for 20 hours of model time, in 20 ms ticks: seven million
  // cycles, far past where an unwrapped phase in radians loses precision.
  auto lfo = make(Drift, 0.01f, false, 0.f);
  const int64_t dt = int64_t(flicks_per_second / 50);
  int64_t t = 0;
  for(int i = 0; i < 20 * 3600 * 50; i++, t += dt)
    tick_free(lfo, t, t + dt);
  CHECK(lfo.cycles > 7'000'000);
  CHECK(lfo.phase >= 0.);
  CHECK(lfo.phase < 1.);

  // Still smooth at fine resolution: 0.1 ms ticks over a few cycles
  const int64_t fine = int64_t(flicks_per_second / 10000);
  float prev = tick_free(lfo, t, t + fine);
  t += fine;
  float worst = 0.f;
  for(int i = 0; i < 1000; i++, t += fine)
  {
    const float v = tick_free(lfo, t, t + fine);
    worst = std::max(worst, std::abs(v - prev));
    prev = v;
  }
  CHECK(worst < 0.05f);
}

TEST_CASE("LFO v3: no jump in the drift after a million radians", "[fx][lfo]")
{
  // 10 ms period: a million radians of phase is 1591.55 s. Coarse ticks up to
  // just before, then fine ones across it.
  auto lfo = make(Drift, 0.01f, false, 0.f);
  const int64_t second = int64_t(flicks_per_second);
  int64_t t = 0;
  for(; t < 1591 * second; t += second)
    tick_free(lfo, t, t + second);
  const int64_t fine = int64_t(flicks_per_second / 10000);
  float prev = tick_free(lfo, t, t + fine);
  t += fine;
  float worst = 0.f;
  for(int i = 0; i < 10000; i++, t += fine)
  {
    const float v = tick_free(lfo, t, t + fine);
    worst = std::max(worst, std::abs(v - prev));
    prev = v;
  }
  CHECK(worst < 0.05f);
}

TEST_CASE("LFO v3: the noises and drift stay in [-1; 1]", "[fx][lfo]")
{
  for(auto w : {Noise1, Noise2, Noise3, Drift, SampleAndHoldEveryTick})
  {
    auto lfo = make(w, 0.05f, false, 1.f);
    const int64_t dt = int64_t(flicks_per_second / 100);
    double sum = 0.;
    int n = 0;
    for(int i = 0; i < 20000; i++)
    {
      const float v = tick_free(lfo, i * dt, (i + 1) * dt);
      REQUIRE(v >= -1.f);
      REQUIRE(v <= 1.f);
      sum += v;
      n++;
    }
    // Centered: no half of the range left out
    INFO("waveform " << int(w));
    CHECK(std::abs(sum / n) < 0.15);
  }
}
