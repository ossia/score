// The LFO presets of the default package load onto the LFO and draw the
// curves they are named for: one-shot ramps and swells that end and stay
// silent, and the classic sweeps, tremolos and random steps.

#include "MidiPresetHarness.hpp"

#include <Fx/LFO_v3.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <optional>
#include <vector>

using Catch::Approx;
using namespace Nodes::LFO::v3;

namespace
{
constexpr int64_t flicks_per_second = 705'600'000;

struct sample
{
  double t{}; // seconds since the start of the run
  float v{};
};

struct driver
{
  Node lfo;
  //! Set before the preset is applied: a synced period is converted at it.
  double tempo = 120.;
  bool synced{};

  //! Plays `seconds` in ticks of `tick` seconds from the quarter `q_start`;
  //! returns the values sent.
  std::vector<sample> play(double seconds, double tick = 0.01, double q_start = 0.)
  {
    std::vector<sample> res;
    const int n = int(seconds / tick + 0.5);
    for(int i = 0; i < n; i++)
    {
      const double t0 = i * tick, t1 = (i + 1) * tick;
      halp::tick_flicks tk{};
      tk.frames = 64;
      tk.tempo = tempo;
      tk.signature = {4, 4};
      tk.start_in_flicks = int64_t(t0 * flicks_per_second);
      tk.end_in_flicks = int64_t(t1 * flicks_per_second);
      tk.start_position_in_quarters = q_start + t0 * tempo / 60.;
      tk.end_position_in_quarters = q_start + t1 * tempo / 60.;
      lfo.outputs.out.value.reset();
      lfo(tk);
      lfo.inputs.retrigger.value.reset();
      if(lfo.outputs.out.value)
        res.push_back({t0, *lfo.outputs.out.value});
    }
    return res;
  }
};

std::map<QString, midi_presets::preset> load_all()
{
  std::map<QString, midi_presets::preset> res;
  for(auto& p : midi_presets::load("LFO", "0b1b1816-c33e-4796-a16d-5aab27fe600f"))
    res[p.name] = p;
  return res;
}

void apply(driver& d, const std::map<QString, midi_presets::preset>& all, const char* name)
{
  auto it = all.find(QString::fromUtf8(name));
  INFO(name);
  REQUIRE(it != all.end());
  const auto errors = midi_presets::apply(d.lfo.inputs, it->second, d.tempo);
  INFO(errors.join("; ").toStdString());
  REQUIRE(errors.isEmpty());
  d.synced = d.lfo.inputs.period.sync;
}

bool in_unit_range(const std::vector<sample>& s)
{
  return std::all_of(s.begin(), s.end(), [](auto& x) {
    return x.v >= -1e-4f && x.v <= 1.f + 1e-4f;
  });
}

//! A one-shot ramp: `ticks` values from `from` to `to`, monotonic, then silence.
void check_ramp(const std::vector<sample>& s, int ticks, float from, float to)
{
  REQUIRE(int(s.size()) == ticks);
  CHECK(s.front().v == Approx(from).margin(1e-3));
  CHECK(s.back().v == Approx(to).margin(1e-3));
  for(std::size_t i = 1; i < s.size(); i++)
  {
    if(to > from)
      CHECK(s[i].v >= s[i - 1].v);
    else
      CHECK(s[i].v <= s[i - 1].v);
  }
  // Halfway through, halfway there.
  CHECK(s[ticks / 2].v == Approx((from + to) / 2.f).margin(0.01));
}
}

TEST_CASE("LFO presets load", "[presets][lfo]")
{
  const auto all = load_all();
  REQUIRE(all.size() >= 8);
  for(auto& [name, p] : all)
  {
    INFO(name.toStdString());
    driver d;
    apply(d, all, name.toUtf8().constData());
    const auto s = d.play(2.);
    CHECK(!s.empty());
    CHECK(in_unit_range(s));
  }
}

TEST_CASE("LFO preset: one shot rise over a bar", "[presets][lfo]")
{
  const auto all = load_all();
  driver d;
  apply(d, all, "One shot rise over a bar");
  CHECK(d.lfo.inputs.one_shot.value);
  // A 4/4 bar at 120 BPM: 2 s, 200 ticks of 10 ms, then nothing.
  check_ramp(d.play(3.), 200, 0.f, 1.f);

  SECTION("Retrigger runs it again")
  {
    d.lfo.inputs.retrigger.value.emplace();
    check_ramp(d.play(3.), 200, 0.f, 1.f);
  }
}

TEST_CASE("LFO preset: one shot rise follows the tempo", "[presets][lfo]")
{
  const auto all = load_all();
  driver d;
  d.tempo = 60.;
  apply(d, all, "One shot rise over a bar");
  check_ramp(d.play(5.), 400, 0.f, 1.f);
}

TEST_CASE("LFO preset: one shot fall over a bar", "[presets][lfo]")
{
  const auto all = load_all();
  driver d;
  apply(d, all, "One shot fall over a bar");
  check_ramp(d.play(3.), 200, 1.f, 0.f);
}

TEST_CASE("LFO preset: one shot fade in over 4 seconds", "[presets][lfo]")
{
  const auto all = load_all();
  driver d;
  d.tempo = 87.;
  apply(d, all, "One shot fade in over 4 seconds");
  CHECK_FALSE(d.synced);
  check_ramp(d.play(6.), 400, 0.f, 1.f);
}

TEST_CASE("LFO preset: one shot sine swell over two bars", "[presets][lfo]")
{
  const auto all = load_all();
  driver d;
  apply(d, all, "One shot sine swell over two bars");
  // Two bars at 120 BPM: 4 s.
  const auto s = d.play(6.);
  REQUIRE(s.size() == 400);
  CHECK(s.front().v == Approx(0.f).margin(1e-3));
  CHECK(s.back().v == Approx(0.f).margin(1e-3));
  const auto peak
      = std::max_element(s.begin(), s.end(), [](auto& a, auto& b) { return a.v < b.v; });
  CHECK(peak->v == Approx(1.f).margin(1e-3));
  CHECK(peak->t == Approx(2.).margin(0.011));
  // Rises to the peak, then falls.
  for(auto it = s.begin() + 1; it != s.end(); ++it)
  {
    if(it->t <= peak->t)
      CHECK(it->v >= (it - 1)->v);
    else
      CHECK(it->v <= (it - 1)->v);
  }
}

TEST_CASE("LFO preset: slow sine sweep", "[presets][lfo]")
{
  const auto all = load_all();
  driver d;
  apply(d, all, "Slow sine sweep");
  CHECK_FALSE(d.synced);
  // 8 s per cycle, whatever the tempo: middle, top at 2 s, bottom at 6 s.
  d.tempo = 140.;
  const auto s = d.play(20.);
  REQUIRE(s.size() == 2000);
  CHECK(in_unit_range(s));
  CHECK(s[0].v == Approx(0.5f).margin(1e-3));
  CHECK(s[200].v == Approx(1.f).margin(1e-3));
  CHECK(s[400].v == Approx(0.5f).margin(1e-3));
  CHECK(s[600].v == Approx(0.f).margin(1e-3));
  CHECK(s[1000].v == Approx(1.f).margin(1e-3));
}

TEST_CASE("LFO preset: square tremolo on eighths", "[presets][lfo]")
{
  const auto all = load_all();
  driver d;
  apply(d, all, "Square tremolo on eighths");
  // An eighth at 120 BPM: 250 ms. Ticks of 1/64 s divide it evenly.
  const auto s = d.play(2., 1. / 64.);
  REQUIRE(s.size() == 128);
  int rising = 0;
  for(std::size_t i = 0; i < s.size(); i++)
  {
    CHECK((s[i].v == Approx(0.f).margin(1e-5) || s[i].v == Approx(1.f).margin(1e-5)));
    if(i > 0 && s[i].v > 0.5f && s[i - 1].v < 0.5f)
      rising++;
  }
  // On for the first half of each eighth.
  CHECK(s[0].v == Approx(1.f));
  CHECK(s[8].v == Approx(0.f));
  CHECK(s[16].v == Approx(1.f));
  CHECK(rising == 7);
}

TEST_CASE("LFO preset: random steps on sixteenths", "[presets][lfo]")
{
  const auto all = load_all();
  driver d;
  apply(d, all, "Random steps on sixteenths");
  // A new value at each sixteenth (125 ms at 120 BPM), sent only then.
  const auto s = d.play(2., 1. / 64.);
  CHECK(in_unit_range(s));
  REQUIRE(s.size() >= 16);
  REQUIRE(s.size() <= 17);
  for(std::size_t i = 1; i < s.size(); i++)
    CHECK(s[i].t - s[i - 1].t == Approx(0.125).margin(1. / 64. + 1e-9));
}

TEST_CASE("LFO preset: triangle over four bars locked to bars", "[presets][lfo]")
{
  const auto all = load_all();
  SECTION("starting at bar 2 picks up a quarter of the way")
  {
    driver d;
    apply(d, all, "Triangle over four bars locked to bars");
    // Four bars: 16 quarters. From quarter 4, the triangle is at its top.
    const auto s = d.play(0.5, 0.01, 4.);
    REQUIRE(!s.empty());
    CHECK(s.front().v == Approx(1.f).margin(1e-3));
  }
  SECTION("from the start")
  {
    driver d;
    apply(d, all, "Triangle over four bars locked to bars");
    // 16 quarters at 120 BPM: 8 s. Middle, top at 2 s, middle at 4 s, bottom at 6 s.
    const auto s = d.play(8.);
    REQUIRE(s.size() == 800);
    CHECK(s[0].v == Approx(0.5f).margin(1e-3));
    CHECK(s[200].v == Approx(1.f).margin(1e-3));
    CHECK(s[400].v == Approx(0.5f).margin(1e-3));
    CHECK(s[600].v == Approx(0.f).margin(1e-3));
  }
}
