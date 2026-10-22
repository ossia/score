#include <AvndProcesses/MIDIClockEngine.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

using Catch::Approx;
using namespace mtk::midi_clock;

namespace
{
constexpr int64_t ms = 1'000'000;
constexpr int64_t sec = 1'000'000'000;

struct sink
{
  std::vector<message> out;
  auto fn()
  {
    return [this](const message& m) { out.push_back(m); };
  }
  int count(uint8_t status) const
  {
    return int(std::count_if(
        out.begin(), out.end(), [=](const message& m) { return m.bytes[0] == status; }));
  }
  std::vector<int64_t> dates(uint8_t status) const
  {
    std::vector<int64_t> d;
    for(auto& m : out)
      if(m.bytes[0] == status)
        d.push_back(m.at);
    return d;
  }
};

int64_t period(double bpm, double factor = 1.)
{
  return int64_t(60e9 / (bpm * factor * 24.));
}
}

TEST_CASE("tap tempo averages up to 32 taps", "[midi_sync_out]")
{
  tap_tempo t;
  CHECK(t.tempo() == 0.);
  t.tap(0);
  CHECK(t.count() == 1);
  CHECK(t.tempo() == 0.);
  t.tap(500 * ms);
  CHECK(t.tempo() == Approx(120.));
  t.tap(1000 * ms);
  t.tap(1500 * ms);
  CHECK(t.count() == 4);
  CHECK(t.tempo() == Approx(120.));

  SECTION("only the last 32 taps count")
  {
    tap_tempo r;
    int64_t now = 0;
    for(int i = 0; i < 10; i++, now += 400 * ms)
      r.tap(now);
    for(int i = 0; i < 32; i++, now += 600 * ms)
      r.tap(now);
    CHECK(r.count() == 32);
    CHECK(r.tempo() == Approx(100.));
  }

  SECTION("3 s without a tap starts a new series")
  {
    t.tap(1500 * ms + 3001 * ms);
    CHECK(t.count() == 1);
    t.tap(1500 * ms + 3001 * ms + 250 * ms);
    CHECK(t.count() == 2);
    CHECK(t.tempo() == Approx(240.));
  }

  SECTION("a tap 3 s after the previous one is still in the series")
  {
    t.tap(1500 * ms + 3000 * ms);
    CHECK(t.count() == 5);
  }

  SECTION("tempo is clamped")
  {
    tap_tempo f;
    f.tap(0);
    f.tap(50 * ms);
    CHECK(f.tempo() == Approx(max_tempo));
  }
}

TEST_CASE("free clock start, stop, continue", "[midi_sync_out]")
{
  free_clock c;
  free_clock_params p;
  sink s;

  c.process(0, p, s.fn());
  CHECK(s.out.empty());
  CHECK(c.next_deadline(p) == free_clock::never);

  c.command(transport_command::Start, 10 * ms, p, s.fn());
  CHECK(c.starting());
  c.process(10 * ms, p, s.fn());
  REQUIRE(s.out.size() == 3);
  CHECK(s.out[0].bytes[0] == 0xF2);
  CHECK(s.out[0].bytes[1] == 0);
  CHECK(s.out[0].bytes[2] == 0);
  CHECK(s.out[0].size == 3);
  CHECK(s.out[1].bytes[0] == 0xFA);
  CHECK(s.out[2].bytes[0] == 0xF8);
  CHECK(s.out[2].at == 10 * ms);
  CHECK(c.running());

  // One second at 120 BPM: 48 clocks after the first one, on exact dates
  // whatever the calls' timing.
  for(int64_t t = 10 * ms; t <= 1010 * ms; t += 7 * ms + 333)
    c.process(t, p, s.fn());
  c.process(1010 * ms, p, s.fn());
  auto d = s.dates(0xF8);
  REQUIRE(d.size() == 49);
  for(std::size_t i = 0; i < d.size(); i++)
    CHECK(d[i] == 10 * ms + int64_t(i) * period(120.));
  CHECK(c.next_deadline(p) == d.back() + period(120.));
  CHECK(c.song_position() == 49 / 6);

  // Stop keeps the position.
  s.out.clear();
  c.command(transport_command::Stop, 1011 * ms, p, s.fn());
  REQUIRE(s.out.size() == 1);
  CHECK(s.out[0].bytes[0] == 0xFC);
  CHECK(s.out[0].at == 1011 * ms);
  c.process(3 * sec, p, s.fn());
  CHECK(s.out.size() == 1);
  CHECK(c.next_deadline(p) == free_clock::never);
  CHECK(c.song_position() == 8);

  // Continue from there.
  s.out.clear();
  c.command(transport_command::Continue, 3 * sec, p, s.fn());
  c.process(3 * sec, p, s.fn());
  REQUIRE(s.out.size() == 3);
  CHECK(s.out[0].bytes[0] == 0xF2);
  CHECK(s.out[0].bytes[1] == 8);
  CHECK(s.out[1].bytes[0] == 0xFB);
  CHECK(s.out[2].bytes[0] == 0xF8);

  // Continue while running does nothing.
  s.out.clear();
  c.command(transport_command::Continue, 3 * sec + ms, p, s.fn());
  CHECK(c.running());
  CHECK(s.out.empty());

  // Start while running restarts from 0.
  c.command(transport_command::Start, 4 * sec, p, s.fn());
  s.out.clear();
  c.process(4 * sec, p, s.fn());
  REQUIRE(s.out.size() == 3);
  CHECK(s.out[0].bytes[1] == 0);
  CHECK(s.out[1].bytes[0] == 0xFA);
  CHECK(c.song_position() == 0);
}

TEST_CASE("free clock without clock or transport", "[midi_sync_out]")
{
  free_clock c;
  free_clock_params p;
  sink s;

  SECTION("transport only")
  {
    p.send_clock = false;
    c.command(transport_command::Start, 0, p, s.fn());
    c.process(sec, p, s.fn());
    CHECK(s.count(0xF8) == 0);
    CHECK(s.count(0xFA) == 1);
    CHECK(c.song_position() == 49 / 6);
  }

  SECTION("clock only")
  {
    p.send_transport = false;
    c.command(transport_command::Start, 0, p, s.fn());
    c.process(sec, p, s.fn());
    CHECK(s.count(0xF8) == 49);
    CHECK(s.count(0xFA) == 0);
    CHECK(s.count(0xF2) == 0);
    c.command(transport_command::Stop, sec, p, s.fn());
    CHECK(s.count(0xFC) == 0);
  }
}

TEST_CASE("tap count-in starts the clock on the next beat", "[midi_sync_out]")
{
  free_clock c;
  free_clock_params p;
  p.bpm = 90.;
  sink s;

  // 100 BPM taps.
  c.tap(0, p);
  c.tap(600 * ms, p);
  c.tap(1200 * ms, p);
  CHECK(!c.starting());
  CHECK(c.tempo(p) == Approx(100.));
  c.process(5 * sec, p, s.fn());
  CHECK(s.out.empty());

  free_clock c2;
  c2.tap(0, p);
  c2.tap(600 * ms, p);
  c2.tap(1200 * ms, p);
  c2.tap(1800 * ms, p);
  CHECK(c2.starting());
  CHECK(c2.count_in_pending());
  CHECK(c2.next_deadline(p) == 2400 * ms);

  c2.process(2399 * ms, p, s.fn());
  CHECK(s.out.empty());
  c2.process(2400 * ms, p, s.fn());
  REQUIRE(s.out.size() == 3);
  CHECK(s.out[1].bytes[0] == 0xFA);
  CHECK(s.out[1].at == 2400 * ms);
  CHECK(c2.running());
  CHECK(c2.next_deadline(p) == 2400 * ms + period(100.));

  // Taps while running change the tempo; this series starts over after the
  // 3 s pause.
  c2.tap(5000 * ms, p);
  c2.tap(5500 * ms, p);
  CHECK(c2.taps().count() == 2);
  CHECK(c2.tempo(p) == Approx(120.));
  CHECK(c2.running());

  // Stop clears the series: one tap does not start again.
  c2.command(transport_command::Stop, 5600 * ms, p, s.fn());
  c2.tap(5700 * ms, p);
  CHECK(c2.taps().count() == 1);
  CHECK(!c2.starting());

  // Changing the BPM control drops the tapped tempo.
  CHECK(c2.tempo(p) == Approx(120.));
  p.bpm = 140.;
  c2.process(6 * sec, p, s.fn());
  CHECK(c2.tempo(p) == Approx(140.));
}

TEST_CASE("free clock count-in reschedules on extra taps", "[midi_sync_out]")
{
  free_clock c;
  free_clock_params p;
  for(int i = 0; i < 4; i++)
    c.tap(i * 500 * ms, p);
  CHECK(c.next_deadline(p) == 2000 * ms);
  c.tap(1900 * ms, p);
  CHECK(c.starting());
  CHECK(c.next_deadline(p) == 1900 * ms + int64_t(c.taps().interval_ns()));
}

TEST_CASE("nudge and clock rate", "[midi_sync_out]")
{
  CHECK(clock_rate_factor(0) == 1.);
  CHECK(clock_rate_factor(1) == 0.25);
  CHECK(clock_rate_factor(3) == 0.5);
  CHECK(clock_rate_factor(4) == 2.);
  CHECK(clock_rate_factor(6) == 4.);
  CHECK(clock_rate_factor(99) == 1.);

  free_clock c;
  free_clock_params p;
  sink s;
  c.command(transport_command::Start, 0, p, s.fn());
  c.process(0, p, s.fn());

  SECTION("nudge up")
  {
    p.nudge = 1;
    CHECK(c.next_deadline(p) == period(120., 1.1));
    c.process(sec, p, s.fn());
    CHECK(s.count(0xF8) == 1 + int(sec / period(120., 1.1)));
    p.nudge = 0;
    const auto last = s.dates(0xF8).back();
    CHECK(c.next_deadline(p) == last + period(120.));
  }
  SECTION("nudge down")
  {
    p.nudge = -1;
    CHECK(c.next_deadline(p) == period(120., 0.9));
  }
  SECTION("x2")
  {
    p.rate = 2.;
    c.process(sec, p, s.fn());
    CHECK(s.count(0xF8) == 97);
  }
  SECTION("x1/2")
  {
    p.rate = 0.5;
    c.process(sec, p, s.fn());
    CHECK(s.count(0xF8) == 25);
  }
}

TEST_CASE("free clock offset", "[midi_sync_out]")
{
  free_clock c;
  free_clock_params p;
  sink s;

  SECTION("positive offset delays the start")
  {
    p.offset_ms = 100.;
    c.command(transport_command::Start, 0, p, s.fn());
    c.process(99 * ms, p, s.fn());
    CHECK(s.out.empty());
    c.process(100 * ms, p, s.fn());
    CHECK(s.count(0xFA) == 1);
    CHECK(s.dates(0xF8).front() == 100 * ms);
  }

  SECTION("negative offset counts as 0")
  {
    p.offset_ms = -100.;
    c.command(transport_command::Start, 0, p, s.fn());
    c.process(0, p, s.fn());
    CHECK(s.count(0xFA) == 1);
  }

  SECTION("changing the offset while running shifts the phase")
  {
    c.command(transport_command::Start, 0, p, s.fn());
    c.process(0, p, s.fn());
    p.offset_ms = 50.;
    c.process(0, p, s.fn());
    CHECK(c.next_deadline(p) == period(120.) + 50 * ms);
    c.process(sec, p, s.fn());
    for(auto d : s.dates(0xF8))
      CHECK((d == 0 || (d - 50 * ms) % period(120.) == 0));

    // 30 ms earlier: more than a clock period at 120 BPM, so clocks are
    // overdue at once, on the new grid.
    const auto before = s.count(0xF8);
    p.offset_ms = 20.;
    c.process(sec, p, s.fn());
    const auto d = s.dates(0xF8);
    REQUIRE(int(d.size()) > before);
    for(std::size_t i = before; i < d.size(); i++)
      CHECK((d[i] - 20 * ms) % period(120.) == 0);
    CHECK((c.next_deadline(p) - 20 * ms) % period(120.) == 0);
  }
}

namespace
{
// The clock generation of MIDISyncOut's audio thread before rate, nudge and
// offset existed: timeline_clock with default controls must match it.
struct reference_clock
{
  double last_clock_time = 0.0;
  std::vector<int64_t> out;
  void process(double current_time, double frame_duration, int frames, float tempo)
  {
    const double seconds_per_tick = 60.0 / (tempo * 24.0);
    double buffer_start_time = current_time;
    double buffer_end_time = current_time + frame_duration;
    double next_tick_time = last_clock_time + seconds_per_tick;
    while(next_tick_time < buffer_end_time)
    {
      if(next_tick_time >= buffer_start_time)
      {
        int64_t sample_offset
            = ((next_tick_time - buffer_start_time) / frame_duration) * frames;
        sample_offset = std::clamp<int64_t>(sample_offset, 0, frames - 1);
        out.push_back(sample_offset);
      }
      last_clock_time = next_tick_time;
      next_tick_time += seconds_per_tick;
    }
  }
};

struct timeline_run
{
  timeline_clock clk;
  std::vector<int64_t> stamps;
  int64_t count{};
  void process(double t, double dur, int frames, float tempo, double factor, double offset)
  {
    clk.process(t, dur, frames, tempo, factor, offset, [&](int64_t ts) {
      if(frames > 0)
      {
        REQUIRE(ts >= 0);
        REQUIRE(ts < frames);
      }
      stamps.push_back(ts);
      count++;
    });
  }
};
}

TEST_CASE("timeline clock matches the score clock by default", "[midi_sync_out]")
{
  reference_clock ref;
  timeline_run run;
  const double rate = 48000.;
  double t = 0.;
  float tempo = 120.f;
  for(int i = 0; i < 2000; i++)
  {
    const int frames = (i % 7 == 3) ? 0 : 64 + (i * 37) % 900;
    const double dur = frames / rate;
    if(i == 500)
      tempo = 97.5f;
    if(i == 900)
      t += 3.21; // seek
    if(i == 1300)
      tempo = 173.f;
    ref.process(t, dur, frames, tempo);
    run.process(t, dur, frames, tempo, 1., 0.);
    t += dur;
  }
  CHECK(run.stamps == ref.out);
  CHECK(run.clk.clock_tick_count == ref.out.size());
}

TEST_CASE("timeline clock rate, nudge and offset", "[midi_sync_out]")
{
  const double rate = 48000.;
  const int frames = 512;
  const double dur = frames / rate;

  auto run_for = [&](double seconds, double factor, double offset, timeline_run& r,
                     double start = 0.) {
    double t = start;
    while(t + dur <= start + seconds + 1e-12)
    {
      r.process(t, dur, frames, 120.f, factor, offset);
      t += dur;
    }
    return t;
  };

  SECTION("x4")
  {
    timeline_run a, b;
    run_for(2.0, 1., 0., a);
    run_for(2.0, 4., 0., b);
    CHECK(b.count == Approx(4 * a.count).margin(4));
  }

  SECTION("positive offset delays")
  {
    timeline_run a;
    // First clock at 1/48 s, delayed by 100 ms.
    double t = 0.;
    int64_t first_frame = -1;
    int64_t now = 0;
    while(t < 0.5 && first_frame < 0)
    {
      a.process(t, dur, frames, 120.f, 1., 0.1);
      if(!a.stamps.empty())
        first_frame = now + a.stamps.front();
      t += dur;
      now += frames;
    }
    CHECK(first_frame == Approx((0.1 + 1. / 48.) * rate).margin(1));
  }

  SECTION("negative offset sends the first clocks at once, then early")
  {
    timeline_run a;
    a.process(0., dur, frames, 120.f, 1., -0.1);
    // Clocks in [0, 100 ms + one tick): 1/48 s apart, the first at 1/48 s.
    const int expected = int((0.1 + dur) * 48.);
    CHECK(a.count == expected);
    CHECK(std::count(a.stamps.begin(), a.stamps.end(), 0) >= int(0.1 * 48.));
  }

  SECTION("no clock is lost when the offset decreases")
  {
    timeline_run a, ref;
    const double end = run_for(1.0, 1., 0.2, a);
    run_for(1.0, 1., 0.2, ref);
    run_for(1.0, 1., -0.05, a, end);
    run_for(1.0, 1., 0.2, ref, end);
    // Moving 250 ms earlier sends 250 ms worth of clocks more by now.
    CHECK(a.count - ref.count == Approx(0.25 * 48.).margin(1));
  }

  SECTION("nudge up runs ahead and stays ahead")
  {
    timeline_run a, ref;
    double t = run_for(1.0, 1.1, 0., a);
    run_for(1.0, 1., 0., ref);
    run_for(1.0, 1., 0., a, t);
    run_for(1.0, 1., 0., ref, t);
    CHECK(a.count - ref.count == Approx(48. * 0.1).margin(1));
  }
}
