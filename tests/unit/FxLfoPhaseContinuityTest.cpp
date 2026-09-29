// LFO v3: the waveform at a given moment does not depend on how the timeline
// was cut into buffers, follows the tempo when synced, from where playback
// started or locked to the bars; the phase knob turns a whole cycle in
// degrees, and the jitter moves it by up to that many degrees; and the drift
// never repeats or jumps however long it runs.

#include <Fx/LFO_v3.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

using Catch::Approx;
using namespace Nodes::LFO::v3;

namespace
{
constexpr double flicks_per_second = 705600000.;

Node make(Waveform w, float period, bool sync, float shape = 0.5f, bool locked = false)
{
  Node lfo;
  lfo.inputs.period.value = period;
  lfo.inputs.period.sync = sync;
  lfo.inputs.lock_to_bars.value = locked;
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

TEST_CASE("LFO v3: synced, the phase follows the musical position", "[fx][lfo]")
{
  const bool locked = GENERATE(false, true);
  INFO("locked to the bars: " << locked);

  SECTION("through tempo changes, no drift and no jump")
  {
    // A bar-long cycle, the tempo sweeping from 60 to 200 BPM: after exactly
    // 64 bars the cycle is back at its start.
    double q = 0.;
    double tempo = 60.;
    auto lfo = make(Triangle, float(4. * 60. / tempo), true, 0.5f, locked);
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
    auto lfo = make(Sin, 60.f / 120.f, true, 0.5f, locked);
    tick_synced(lfo, 0., 0.3, 120.);
    lfo.inputs.retrigger.value.emplace();
    CHECK(tick_synced(lfo, 0.3, 0.4, 120.) == Approx(0.).margin(1e-6));
    lfo.inputs.retrigger.value.reset();
    tick_synced(lfo, 0.4, 0.55, 120.);
    CHECK(tick_synced(lfo, 0.55, 0.6, 120.) == Approx(1.).margin(1e-5));
  }
}

TEST_CASE("LFO v3: synced, the cycles count from the start unless locked to the bars", "[fx][lfo]")
{
  SECTION("not locked by default")
  {
    Node lfo;
    CHECK_FALSE(lfo.inputs.lock_to_bars.value);
  }

  SECTION("not locked: playback starting mid-bar starts the cycle there")
  {
    // A quarter-note cycle; playback starts at quarter 2.25.
    auto lfo = make(Sin, 60.f / 120.f, true);
    CHECK(tick_synced(lfo, 2.25, 2.5, 120.) == Approx(0.).margin(1e-5));
    CHECK(tick_synced(lfo, 2.5, 2.75, 120.) == Approx(1.).margin(1e-5));
    CHECK(tick_synced(lfo, 2.75, 3., 120.) == Approx(0.).margin(1e-5));
    CHECK(tick_synced(lfo, 3., 3.25, 120.) == Approx(-1.).margin(1e-5));
  }

  SECTION("not locked: a new period carries on from where the cycle is")
  {
    auto lfo = make(Triangle, 60.f / 120.f, true);
    tick_synced(lfo, 0., 0.25, 120.); // a quarter of a cycle: the peak
    lfo.inputs.period.value = 4.f * 60.f / 120.f; // a bar
    CHECK(tick_synced(lfo, 0.25, 0.5, 120.) == Approx(1.).margin(1e-5));
    // Then a sixteenth of a cycle every quarter
    CHECK(tick_synced(lfo, 0.5, 0.75, 120.) == Approx(0.75).margin(1e-5));
  }

  SECTION("locked: playback starting mid-bar is still on the beats")
  {
    auto lfo = make(Sin, 60.f / 120.f, true, 0.5f, true);
    CHECK(tick_synced(lfo, 2.25, 2.5, 120.) == Approx(1.).margin(1e-5));
    CHECK(tick_synced(lfo, 2.5, 2.75, 120.) == Approx(0.).margin(1e-5));
    CHECK(tick_synced(lfo, 3., 3.25, 120.) == Approx(0.).margin(1e-5));
  }

  SECTION("unlocking mid-play carries on from where the cycle is")
  {
    // Playback started mid-bar, locked: a quarter-note cycle on the beats.
    // Unlocked at quarter 4.3, the next tick moves by one tick only.
    auto lfo = make(Sin, 60.f / 120.f, true, 0.5f, true);
    float prev = tick_synced(lfo, 2.3, 2.35, 120.);
    for(int i = 1; i < 40; i++)
      prev = tick_synced(lfo, 2.3 + i * 0.05, 2.35 + i * 0.05, 120.);
    // sin moves by at most 2 pi x 0.05 per tick of 0.05 cycle
    const float max_step = float(2. * 3.14159265358979 * 0.05) + 1e-4f;
    lfo.inputs.lock_to_bars.value = false;
    float worst = 0.f;
    for(int i = 40; i < 120; i++)
    {
      const float v = tick_synced(lfo, 2.3 + i * 0.05, 2.35 + i * 0.05, 120.);
      worst = std::max(worst, std::abs(v - prev));
      prev = v;
    }
    CHECK(worst <= max_step);
    // Still on the beats: it went on from the locked position
    CHECK(tick_synced(lfo, 8.3, 8.35, 120.) == Approx(std::sin(2. * 3.14159265358979 * 0.3)).margin(1e-4));
  }

  SECTION("locked: two started at different dates play in step")
  {
    auto a = make(Saw, 2.f * 60.f / 120.f, true, 0.5f, true);
    auto b = make(Saw, 2.f * 60.f / 120.f, true, 0.5f, true);
    for(int i = 0; i < 16; i++)
      tick_synced(a, i * 0.125, (i + 1) * 0.125, 120.);
    for(int i = 16; i < 64; i++)
    {
      const double q0 = i * 0.125, q1 = q0 + 0.125;
      CHECK(tick_synced(a, q0, q1, 120.) == Approx(tick_synced(b, q0, q1, 120.)).margin(1e-6));
    }
  }
}

namespace
{
//! Ticks of a hundredth of a cycle, from the start of the timeline.
struct Timeline
{
  bool sync{};
  bool locked{};
  static constexpr int ticks_per_cycle = 100;
  static constexpr double period = 0.5; // seconds, a quarter at 120 BPM

  std::optional<float> tick(Node& lfo, int i) const
  {
    lfo.outputs.out.value.reset();
    halp::tick_flicks tk{};
    tk.frames = 64;
    if(sync)
    {
      tk.tempo = 120.;
      tk.signature = {4, 4};
      tk.start_position_in_quarters = double(i) / ticks_per_cycle;
      tk.end_position_in_quarters = double(i + 1) / ticks_per_cycle;
    }
    else
    {
      const double dt = period * flicks_per_second / ticks_per_cycle;
      tk.start_in_flicks = int64_t(std::llround(i * dt));
      tk.end_in_flicks = int64_t(std::llround((i + 1) * dt));
    }
    lfo(tk);
    return lfo.outputs.out.value;
  }

  Node make(Waveform w, float phase) const
  {
    auto lfo = ::make(w, float(period), sync, 0.3f, locked);
    lfo.inputs.phase.value = phase;
    return lfo;
  }

  std::vector<std::optional<float>> run(Node& lfo, int n) const
  {
    std::vector<std::optional<float>> out;
    for(int i = 0; i < n; i++)
      out.push_back(tick(lfo, i));
    return out;
  }
};
}

TEST_CASE("LFO v3: the phase knob is the waveform shifted in time", "[fx][lfo]")
{
  // Output with phase p degrees = output with phase 0, p / 360 of a period
  // later. Both are moved by half a tick more, so that no tick starts on an
  // edge of the square or the saw.
  const Timeline tl{.sync = GENERATE(false, true), .locked = GENERATE(false, true)};
  if(!tl.sync && tl.locked)
    return;
  INFO("synced: " << tl.sync << ", locked to the bars: " << tl.locked);

  const double half_tick = 180. / Timeline::ticks_per_cycle;
  for(int k : {1, 13, 25, 50, 63, 99})
  {
    const float phase = float(half_tick + 360. * k / Timeline::ticks_per_cycle);
    const auto plain_phase = float(half_tick);
    constexpr int n = 250;
    INFO("phase " << phase << " degrees, " << k << " ticks ahead");

    for(auto w : {Sin, Triangle, Saw, RampDown, Square, Drift})
    {
      INFO("waveform " << int(w));
      auto shifted = tl.make(w, phase);
      auto plain = tl.make(w, plain_phase);
      plain.seed = shifted.seed;
      const auto a = tl.run(shifted, n);
      const auto b = tl.run(plain, n + k);
      for(int i = 0; i < n; i++)
        CHECK(*a[i] == Approx(*b[i + k]).margin(2e-4));
    }

    // Stepped: the steps fall at the same moments.
    for(auto w : {SquareOnChange, SampleAndHold, SampleAndHoldEveryTick})
    {
      INFO("waveform " << int(w));
      auto shifted = tl.make(w, phase);
      auto plain = tl.make(w, plain_phase);
      const auto a = tl.run(shifted, n);
      const auto b = tl.run(plain, n + k);
      const auto sent = [](const auto& v, int i) {
        return i > 0 && v[i].has_value() && (!v[i - 1] || *v[i] != *v[i - 1]);
      };
      int steps = 0;
      for(int i = 1; i < n; i++)
      {
        const bool sa = w == SampleAndHoldEveryTick ? sent(a, i) : a[i].has_value();
        const bool sb = w == SampleAndHoldEveryTick ? sent(b, i + k)
                                                    : b[i + k].has_value();
        CHECK(sa == sb);
        steps += sa;
        if(w == SquareOnChange && sa && sb)
          CHECK(*a[i] == *b[i + k]);
      }
      CHECK(steps >= 3);
    }
  }
}

TEST_CASE("LFO v3: the phase knob turns a whole cycle, without a jump", "[fx][lfo]")
{
  const Timeline tl{.sync = GENERATE(false, true), .locked = GENERATE(false, true)};
  if(!tl.sync && tl.locked)
    return;
  INFO("synced: " << tl.sync << ", locked to the bars: " << tl.locked);

  // The range is one whole cycle, and its two ends are the same phase.
  Node proto;
  using range = decltype(proto.inputs.phase)::range;
  CHECK(range{}.min == 0.);
  CHECK(range{}.max == 360.);
  CHECK(range{}.init == 0.);

  // At a fixed moment, 20 ticks into the run, the knob swept end to end in
  // 2000 steps: the waveform over one whole cycle, back where it started.
  constexpr int steps = 2000;
  for(auto w : {Sin, Triangle, Saw, RampDown, Square})
  {
    INFO("waveform " << int(w));
    std::vector<float> v;
    for(int s = 0; s <= steps; s++)
    {
      auto lfo = tl.make(w, float(range{}.min + (range{}.max - range{}.min) * s / steps));
      tl.run(lfo, 20);
      v.push_back(*tl.tick(lfo, 20));
    }
    CHECK(v.front() == Approx(v.back()).margin(1e-5));

    // Only the waveform's own jumps: none for the smooth ones, one drop for
    // the saw, one rise for the ramp down, two edges for the square.
    int jumps = 0;
    float lo = v[0], hi = v[0];
    for(int s = 1; s <= steps; s++)
    {
      jumps += std::abs(v[s] - v[s - 1]) > 0.05f;
      lo = std::min(lo, v[s]);
      hi = std::max(hi, v[s]);
    }
    // One whole cycle: all of the waveform's range
    CHECK(lo == Approx(-1.).margin(2e-2));
    CHECK(hi == Approx(1.).margin(2e-2));
    switch(w)
    {
      case Sin:
      case Triangle:
        CHECK(jumps == 0);
        break;
      case Saw:
      case RampDown:
        CHECK(jumps == 1);
        break;
      case Square:
        CHECK(jumps == 2);
        break;
      default:
        break;
    }
  }
}

TEST_CASE("LFO v3: the phase is in degrees, where the cycle starts", "[fx][lfo]")
{
  const auto quarter = int64_t(flicks_per_second / 4);
  SECTION("90 degrees starts a sine on its peak, a quarter of a cycle on")
  {
    auto lfo = make(Sin, 1.f, false);
    lfo.inputs.phase.value = 90.f;
    CHECK(tick_free(lfo, 0, quarter) == Approx(1.).margin(1e-5));
    CHECK(tick_free(lfo, quarter, 2 * quarter) == Approx(0.).margin(1e-5));
    CHECK(tick_free(lfo, 2 * quarter, 3 * quarter) == Approx(-1.).margin(1e-5));
  }

  SECTION("retrigger restarts the cycle at the phase")
  {
    const bool synced = GENERATE(false, true);
    const bool locked = GENERATE(false, true);
    if(!synced && locked)
      return;
    INFO("synced: " << synced << ", locked to the bars: " << locked);
    auto lfo = make(Sin, 60.f / 120.f, synced, 0.5f, locked);
    lfo.inputs.phase.value = 270.f;
    const auto tick = [&](double q0, double q1) {
      // Free: 120 BPM, a quarter note is half a second
      return synced ? tick_synced(lfo, q0, q1, 120.)
                    : tick_free(
                          lfo, int64_t(q0 * flicks_per_second / 2.),
                          int64_t(q1 * flicks_per_second / 2.));
    };
    tick(1.1, 1.3);
    lfo.inputs.retrigger.value.emplace();
    CHECK(tick(1.3, 1.4) == Approx(-1.).margin(1e-5));
    lfo.inputs.retrigger.value.reset();
    tick(1.4, 1.55);
    CHECK(tick(1.55, 1.6) == Approx(0.).margin(1e-5));
  }

  SECTION("beyond the range, the phase wraps")
  {
    for(auto w : {Sin, Triangle, Saw, Square})
    {
      INFO("waveform " << int(w));
      for(float p : {-90.f, 630.f, -450.f})
      {
        INFO("phase " << p);
        auto a = make(w, 0.37f, false, 0.3f);
        auto b = make(w, 0.37f, false, 0.3f);
        a.inputs.phase.value = p;
        b.inputs.phase.value = 270.f;
        const int64_t dt = int64_t(flicks_per_second / 100);
        for(int i = 0; i < 100; i++)
          CHECK(tick_free(a, i * dt, (i + 1) * dt)
                == Approx(tick_free(b, i * dt, (i + 1) * dt)).margin(1e-4));
      }
    }
  }
}

TEST_CASE("LFO v3: the jitter moves the phase by up to that many degrees", "[fx][lfo]")
{
  Node proto;
  using range = decltype(proto.inputs.jitter)::range;
  CHECK(range{}.min == 0.);
  CHECK(range{}.max == 180.);
  CHECK(range{}.init == 0.);

  // A linear saw at the middle of its ramp: a phase moved by d degrees moves
  // the value by 2 d / 360. Empty ticks: the cycle stays at its start.
  const auto spread = [](float jitter) {
    auto lfo = make(Saw, 1.f, false);
    lfo.inputs.jitter.value = jitter;
    float lo = 2.f, hi = -2.f;
    for(int i = 0; i < 4000; i++)
    {
      const float v = tick_free(lfo, 0, 0);
      lo = std::min(lo, v);
      hi = std::max(hi, v);
    }
    return std::pair{lo, hi};
  };
  {
    const auto [lo, hi] = spread(0.f);
    CHECK(lo == Approx(0.).margin(1e-5));
    CHECK(hi == Approx(0.).margin(1e-5));
  }
  {
    // +/- 36 degrees: +/- a tenth of a cycle, +/- 0.2 on the ramp
    const auto [lo, hi] = spread(36.f);
    CHECK(lo >= -0.2f - 1e-4f);
    CHECK(hi <= 0.2f + 1e-4f);
    CHECK(lo < -0.19f);
    CHECK(hi > 0.19f);
  }
  {
    // 180: anywhere in the cycle
    const auto [lo, hi] = spread(180.f);
    CHECK(lo < -0.99f);
    CHECK(hi > 0.99f);
  }
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
