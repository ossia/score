// LFO v3 one shot: a single cycle from the start or from Retrigger, the value
// at the end of the cycle on the tick that completes it, then nothing until
// the next Retrigger. Off, the LFO runs as before.

#include <Fx/LFO_v3.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <optional>
#include <vector>

using Catch::Approx;
using namespace Nodes::LFO::v3;

namespace
{
constexpr int64_t ms = 705600;

Node make(Waveform w, float period_s, bool one_shot)
{
  Node lfo;
  lfo.inputs.period.value = period_s;
  lfo.inputs.period.sync = false;
  lfo.inputs.shape.value = 0.5f;
  lfo.inputs.ampl.value = 1.f;
  lfo.inputs.offset.value = 0.f;
  lfo.inputs.jitter.value = 0.f;
  lfo.inputs.phase.value = 0.f;
  lfo.inputs.waveform.value = w;
  lfo.inputs.one_shot.value = one_shot;
  return lfo;
}

std::optional<float> tick(Node& lfo, int64_t start_ms, int64_t end_ms)
{
  halp::tick_flicks tk{};
  tk.frames = 64;
  tk.start_in_flicks = start_ms * ms;
  tk.end_in_flicks = end_ms * ms;
  lfo.outputs.out.value.reset();
  lfo(tk);
  lfo.inputs.retrigger.value.reset();
  return lfo.outputs.out.value;
}

std::optional<float> tick_synced(Node& lfo, double q0, double q1)
{
  halp::tick_flicks tk{};
  tk.frames = 64;
  tk.tempo = 120.;
  tk.signature = {4, 4};
  tk.start_position_in_quarters = q0;
  tk.end_position_in_quarters = q1;
  lfo.outputs.out.value.reset();
  lfo(tk);
  lfo.inputs.retrigger.value.reset();
  return lfo.outputs.out.value;
}
}

TEST_CASE("LFO v3: one shot off keeps cycling", "[fx][lfo]")
{
  auto a = make(Sin, 0.1f, false);
  int sent = 0;
  for(int i = 0; i < 100; i++)
    sent += tick(a, i * 10, i * 10 + 10).has_value();
  CHECK(sent == 100);
}

TEST_CASE("LFO v3: one shot runs a single cycle", "[fx][lfo]")
{
  // 100 ms period, 10 ms ticks: ticks 0 to 9 cover the cycle
  for(auto w : {Sin, Triangle, Saw, RampDown, Square, Drift})
  {
    auto a = make(w, 0.1f, true);
    auto b = make(w, 0.1f, false);
    b.seed = a.seed;

    for(int i = 0; i < 9; i++)
    {
      const auto va = tick(a, i * 10, i * 10 + 10);
      const auto vb = tick(b, i * 10, i * 10 + 10);
      REQUIRE(va.has_value());
      REQUIRE(vb.has_value());
      CHECK(*va == Approx(*vb).margin(1e-6));
    }

    // The tick completing the cycle sends the value at its very end
    const auto last = tick(a, 90, 100);
    REQUIRE(last.has_value());
    auto c = make(w, 0.1f, false);
    c.seed = a.seed;
    c.phase = 1. - 1e-8;
    const auto end = tick(c, 0, 10);
    REQUIRE(end.has_value());
    CHECK(*last == Approx(*end).margin(1e-4));

    for(int i = 10; i < 50; i++)
      CHECK(!tick(a, i * 10, i * 10 + 10).has_value());

    // Retrigger starts it again
    a.inputs.retrigger.value.emplace();
    int sent = 0;
    for(int i = 50; i < 100; i++)
      sent += tick(a, i * 10, i * 10 + 10).has_value();
    CHECK(sent == 10);
  }
}

TEST_CASE("LFO v3: one shot plays again when the process starts again", "[fx][lfo]")
{
  // A looping interval, or one triggered again, restarts its processes
  auto a = make(Sin, 0.1f, true);
  int first = 0;
  for(int i = 0; i < 30; i++)
    first += tick(a, i * 10, i * 10 + 10).has_value();
  CHECK(first == 10);

  a.start();
  int second = 0;
  for(int i = 30; i < 60; i++)
    second += tick(a, i * 10, i * 10 + 10).has_value();
  CHECK(second == 10);
}

TEST_CASE("LFO v3: one shot ends inside a long tick", "[fx][lfo]")
{
  // The saw starts its cycle at 0 and comes back to 0 at its end
  auto a = make(Saw, 0.1f, true);
  const auto v = tick(a, 0, 250);
  REQUIRE(v.has_value());
  CHECK(*v == Approx(0.f).margin(1e-4));
  CHECK(!tick(a, 250, 260).has_value());
}

TEST_CASE("LFO v3: one shot follows the tempo when synced", "[fx][lfo]")
{
  // 0.5 s at 120 bpm is one quarter note per cycle: four ticks of a sixteenth
  auto a = make(Triangle, 0.5f, true);
  a.inputs.period.sync = true;
  int sent = 0;
  double q = 0.;
  for(int i = 0; i < 16; i++, q += 0.25)
    sent += tick_synced(a, q, q + 0.25).has_value();
  CHECK(sent == 4);

  SECTION("also locked to the bars, counted from the start of playback")
  {
    auto b = make(Triangle, 0.5f, true);
    b.inputs.period.sync = true;
    b.inputs.lock_to_bars.value = true;
    int n = 0;
    double p = 37.;
    for(int i = 0; i < 16; i++, p += 0.25)
      n += tick_synced(b, p, p + 0.25).has_value();
    CHECK(n == 4);
  }
}
