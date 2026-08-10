#include <Advanced/MidiScaler/MidiHumanizer.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <complex>
#include <map>
#include <numbers>
#include <vector>

using Catch::Approx;
using Hum = mtk::MidiHumanizer;

namespace
{
constexpr double rate = 48000.;

struct event
{
  int64_t at{};      //!< absolute sample date
  int type{};        //!< 0x90 note on, 0x80 note off, other = raw status
  int pitch{};
  int velocity{};
};

struct driver
{
  Hum fx;
  int64_t now{};
  std::vector<event> out;

  driver()
  {
    fx.prepare(halp::setup{
        .input_channels = 0, .output_channels = 0, .frames = 4096, .rate = rate});
  }

  void note_on(int note, int vel, int ts = 0, int channel = 1)
  {
    auto m = libremidi::channel_events::note_on(channel, note, vel);
    m.timestamp = ts;
    fx.inputs.midi.push_back(m);
  }

  void note_off(int note, int ts = 0, int channel = 1)
  {
    auto m = libremidi::channel_events::note_off(channel, note, 0);
    m.timestamp = ts;
    fx.inputs.midi.push_back(m);
  }

  //! Run a block, appending whatever came out to `out` with absolute dates.
  void tick(int frames)
  {
    fx.outputs.midi.midi_messages.clear();
    fx(halp::tick_musical{.frames = frames});

    for(const auto& m : fx.outputs.midi.midi_messages)
    {
      REQUIRE(m.timestamp >= 0);
      REQUIRE(m.timestamp < frames);
      out.push_back(event{
          now + m.timestamp, (int)m.get_message_type(),
          m.size() > 1 ? (int)m.bytes[1] : 0, m.size() > 2 ? (int)m.bytes[2] : 0});
    }

    fx.inputs.midi.midi_messages.clear();
    now += frames;
  }

  //! Drain anything still queued.
  void drain(int blocks = 8, int frames = 4096)
  {
    for(int i = 0; i < blocks; i++)
      tick(frames);
  }

  std::vector<event> notes_on() const
  {
    std::vector<event> r;
    for(auto& e : out)
      if(e.type == (int)libremidi::message_type::NOTE_ON)
        r.push_back(e);
    return r;
  }
  std::vector<event> notes_off() const
  {
    std::vector<event> r;
    for(auto& e : out)
      if(e.type == (int)libremidi::message_type::NOTE_OFF)
        r.push_back(e);
    return r;
  }
};

//! Feed one note per block and return the timing deviation applied to each,
//! in samples, relative to the object's own latency.
//!
//! The block must be comfortably larger than latency + a few sigma, so that a
//! note never overtakes the next one; the i-th note-on is then the i-th input.
std::vector<double>
collect_offsets(int count, float colour, float timing_s, int seed, int block = 16384)
{
  driver d;
  auto& in = d.fx.inputs;
  in.timing.value = timing_s;
  in.colour.value = colour;
  in.early.value = 1.f; // symmetric, so nothing is clamped
  in.velocity.value = 0;
  in.length.value = 0.f;
  in.chance.value = 1.f;
  in.seed.value = seed;

  const double latency = 3.0 * std::llround(double(timing_s) * rate);

  for(int i = 0; i < count; i++)
  {
    d.note_on(60, 100, 0);
    d.note_off(60, 64);
    d.tick(block);
  }
  d.drain(4, block);

  const auto ons = d.notes_on();
  std::vector<double> offsets;
  offsets.reserve(ons.size());
  for(std::size_t i = 0; i < ons.size(); i++)
    offsets.push_back(double(ons[i].at - int64_t(i) * block) - latency);
  return offsets;
}

double mean(const std::vector<double>& v)
{
  double s = 0.;
  for(double x : v)
    s += x;
  return v.empty() ? 0. : s / v.size();
}

double stddev(const std::vector<double>& v)
{
  const double m = mean(v);
  double s = 0.;
  for(double x : v)
    s += (x - m) * (x - m);
  return v.size() < 2 ? 0. : std::sqrt(s / (v.size() - 1));
}

//! Power at normalized frequency k/N, by direct evaluation of the DFT bin.
double power_at_bin(const std::vector<double>& x, int k)
{
  const std::size_t N = x.size();
  const double w = 2. * std::numbers::pi * double(k) / double(N);
  double re = 0., im = 0.;
  for(std::size_t n = 0; n < N; n++)
  {
    re += x[n] * std::cos(w * double(n));
    im -= x[n] * std::sin(w * double(n));
  }
  return (re * re + im * im) / double(N);
}

/**
 * Estimate the spectral exponent of a series.
 *
 * The generator is the fractional-difference filter (1 - z^-1)^(-beta/2), whose
 * exact power spectrum is |1 - e^-jw|^-beta = (2 sin(w/2))^-beta. So we regress
 * log(P) against log(2 sin(w/2)): the slope is -beta, exactly, without the
 * low-frequency approximation w^-beta.
 *
 * The lowest bins are skipped: the impulse response is damped by a leak and cut
 * off at 256 taps, which flattens the spectrum at the very lowest frequencies.
 */
double spectral_slope(
    const std::vector<std::vector<double>>& realizations, const std::vector<int>& bins)
{
  const std::size_t N = realizations.front().size();

  double sx = 0., sy = 0., sxx = 0., sxy = 0.;
  int n = 0;
  for(int k : bins)
  {
    double p = 0.;
    for(const auto& r : realizations)
      p += power_at_bin(r, k);
    p /= double(realizations.size());
    if(p <= 0.)
      continue;

    const double w = 2. * std::numbers::pi * double(k) / double(N);
    const double x = std::log(2. * std::sin(w / 2.));
    const double y = std::log(p);

    sx += x;
    sy += y;
    sxx += x * x;
    sxy += x * y;
    ++n;
  }

  return (double(n) * sxy - sx * sy) / (double(n) * sxx - sx * sx);
}
}

TEST_CASE("mtk::MidiHumanizer is a passthrough with everything at zero", "[avnd][midi][humanize]")
{
  driver d;
  auto& in = d.fx.inputs;
  in.timing.value = 0.f;
  in.velocity.value = 0;
  in.length.value = 0.f;
  in.chance.value = 1.f;
  in.early.value = 0.f;

  d.note_on(60, 100, 10);
  d.note_off(60, 500);
  d.note_on(64, 42, 20);
  d.note_off(64, 900);
  d.tick(1024);

  REQUIRE(d.out.size() == 4);
  CHECK(d.out[0].at == 10);
  CHECK(d.out[0].pitch == 60);
  CHECK(d.out[0].velocity == 100);
  CHECK(d.out[1].at == 20);
  CHECK(d.out[1].pitch == 64);
  CHECK(d.out[1].velocity == 42);
  CHECK(d.out[2].at == 500);
  CHECK(d.out[3].at == 900);
}

TEST_CASE("mtk::MidiHumanizer emits in order and never in the past", "[avnd][midi][humanize]")
{
  driver d;
  auto& in = d.fx.inputs;
  in.timing.value = 0.03f;
  in.early.value = 1.f;
  in.velocity.value = 20;
  in.length.value = 0.5f;
  in.seed.value = 7;

  for(int i = 0; i < 64; i++)
  {
    d.note_on(60 + (i % 12), 100, (i % 8) * 64);
    d.note_off(60 + (i % 12), (i % 8) * 64 + 200);
    d.tick(1024);
  }
  d.drain();

  REQUIRE(!d.out.empty());
  for(std::size_t i = 1; i < d.out.size(); i++)
    CHECK(d.out[i - 1].at <= d.out[i].at);
}

TEST_CASE("mtk::MidiHumanizer never plays early when Early is 0", "[avnd][midi][humanize]")
{
  driver d;
  auto& in = d.fx.inputs;
  in.timing.value = 0.02f;
  in.early.value = 0.f;
  in.seed.value = 3;

  for(int i = 0; i < 200; i++)
  {
    d.note_on(60, 100, 0);
    d.note_off(60, 64);
    d.tick(8192);
  }
  d.drain();

  const auto ons = d.notes_on();
  REQUIRE(ons.size() > 150);
  for(std::size_t i = 0; i < ons.size(); i++)
    CHECK(ons[i].at >= int64_t(i) * 8192);
}

TEST_CASE("mtk::MidiHumanizer keeps unit variance across noise colours", "[avnd][midi][humanize]")
{
  // The deviation amount must mean the same thing whatever the colour is,
  // otherwise changing Colour would silently change how loose the part feels.
  const float timing = 0.02f;
  const double sigma_samples = timing * rate;

  for(float colour : {0.f, 1.f, 2.f})
  {
    const auto off = collect_offsets(1500, colour, timing, 11);
    REQUIRE(off.size() > 1400);

    const double s = stddev(off);
    INFO("colour = " << colour << " sigma = " << s << " expected " << sigma_samples);
    CHECK(s == Approx(sigma_samples).epsilon(0.20));
  }
}

TEST_CASE("mtk::MidiHumanizer produces 1/f^beta timing deviations", "[avnd][midi][humanize]")
{
  // The point of the object: human rhythmic fluctuations are long-range
  // correlated (Hennig et al., PLoS ONE 2011), not white. Verify that the
  // generated deviation series really has the requested spectral exponent.
  constexpr int N = 2048;
  constexpr int realizations = 6;

  // Skip the lowest bins: the response is leaky and cut off at 256 taps, so the
  // spectrum flattens below the leak knee. Log-spaced bins keep the regression
  // balanced (and the direct DFT affordable).
  std::vector<int> bins;
  for(double k = 32.; k <= 512.; k *= 1.06)
  {
    const int b = (int)std::lround(k);
    if(bins.empty() || bins.back() != b)
      bins.push_back(b);
  }

  for(float beta : {0.f, 1.f, 2.f})
  {
    std::vector<std::vector<double>> series;
    for(int r = 0; r < realizations; r++)
    {
      auto s = collect_offsets(N, beta, 0.02f, 100 + r);
      REQUIRE(s.size() == N);
      series.push_back(std::move(s));
    }

    const double slope = spectral_slope(series, bins);
    INFO("beta = " << beta << " measured slope = " << slope);
    CHECK(slope == Approx(-double(beta)).margin(0.10));
  }
}

TEST_CASE("mtk::MidiHumanizer keeps note lengths when Length is 0", "[avnd][midi][humanize]")
{
  driver d;
  auto& in = d.fx.inputs;
  in.timing.value = 0.02f;
  in.early.value = 1.f;
  in.length.value = 0.f;
  in.seed.value = 5;

  constexpr int64_t duration = 1000;
  for(int i = 0; i < 100; i++)
  {
    d.note_on(60, 100, 0);
    d.note_off(60, duration);
    d.tick(8192);
  }
  d.drain();

  const auto ons = d.notes_on();
  const auto offs = d.notes_off();
  REQUIRE(ons.size() == offs.size());
  REQUIRE(ons.size() > 90);

  // The note-off inherits the exact displacement of its note-on
  for(std::size_t i = 0; i < ons.size(); i++)
    CHECK(offs[i].at - ons[i].at == duration);
}

TEST_CASE("mtk::MidiHumanizer scales lengths without inverting notes", "[avnd][midi][humanize]")
{
  driver d;
  auto& in = d.fx.inputs;
  in.timing.value = 0.f;
  in.length.value = 1.f; // maximum scaling
  in.seed.value = 9;

  constexpr int64_t duration = 800;
  for(int i = 0; i < 200; i++)
  {
    d.note_on(60, 100, 0);
    d.note_off(60, duration);
    d.tick(8192);
  }
  d.drain();

  const auto ons = d.notes_on();
  const auto offs = d.notes_off();
  REQUIRE(ons.size() == offs.size());

  bool any_different = false;
  for(std::size_t i = 0; i < ons.size(); i++)
  {
    const int64_t len = offs[i].at - ons[i].at;
    CHECK(len >= 1); // never zero-length or inverted
    if(len != duration)
      any_different = true;
  }
  CHECK(any_different);
}

TEST_CASE("mtk::MidiHumanizer drops notes whole", "[avnd][midi][humanize]")
{
  SECTION("chance 0 silences everything, note-offs included")
  {
    driver d;
    d.fx.inputs.chance.value = 0.f;
    d.fx.inputs.seed.value = 2;

    for(int i = 0; i < 50; i++)
    {
      d.note_on(60, 100, 0);
      d.note_off(60, 200);
      d.tick(1024);
    }
    d.drain();
    CHECK(d.out.empty());
  }

  SECTION("partial chance never leaves an orphan note-on or note-off")
  {
    driver d;
    d.fx.inputs.chance.value = 0.5f;
    d.fx.inputs.timing.value = 0.01f;
    d.fx.inputs.early.value = 1.f;
    d.fx.inputs.seed.value = 4;

    for(int i = 0; i < 200; i++)
    {
      d.note_on(60, 100, 0);
      d.note_off(60, 300);
      d.tick(8192);
    }
    d.drain();

    CHECK(d.notes_on().size() == d.notes_off().size());
    CHECK(d.notes_on().size() > 50);
    CHECK(d.notes_on().size() < 150);

    // Strictly alternating on/off: no note is ever left hanging
    int held = 0;
    for(auto& e : d.out)
    {
      if(e.type == (int)libremidi::message_type::NOTE_ON)
        held++;
      else if(e.type == (int)libremidi::message_type::NOTE_OFF)
        held--;
      CHECK(held >= 0);
      CHECK(held <= 1);
    }
    CHECK(held == 0);
  }
}

TEST_CASE("mtk::MidiHumanizer displaces chords as a whole", "[avnd][midi][humanize]")
{
  driver d;
  auto& in = d.fx.inputs;
  in.timing.value = 0.02f;
  in.early.value = 1.f;
  in.chord_window.value = 0.02f; // 960 samples
  in.spread.value = 0.f;
  in.seed.value = 8;

  // Three notes inside the chord window, then one well outside it
  d.note_on(60, 100, 0);
  d.note_on(64, 100, 100);
  d.note_on(67, 100, 200);
  d.note_on(72, 100, 4000);
  d.tick(16384);
  d.drain();

  const auto ons = d.notes_on();
  REQUIRE(ons.size() == 4);

  std::map<int, int64_t> at;
  for(auto& e : ons)
    at[e.pitch] = e.at;

  // Same deviation for the chord: the relative spacing is preserved exactly
  CHECK(at[64] - at[60] == 100);
  CHECK(at[67] - at[60] == 200);

  // The note outside the window got its own draw
  CHECK(at[72] - at[60] != 4000);
}

TEST_CASE("mtk::MidiHumanizer strums a chord by the Spread amount", "[avnd][midi][humanize]")
{
  driver d;
  auto& in = d.fx.inputs;
  in.timing.value = 0.f; // isolate the strum
  in.early.value = 0.f;
  in.chord_window.value = 0.05f;
  in.spread.value = 0.01f; // 480 samples
  in.seed.value = 1;

  d.note_on(60, 100, 0);
  d.note_on(64, 100, 0);
  d.note_on(67, 100, 0);
  d.tick(16384);
  d.drain();

  const auto ons = d.notes_on();
  REQUIRE(ons.size() == 3);
  CHECK(ons[0].at == 0);
  CHECK(ons[1].at == 480);
  CHECK(ons[2].at == 960);
}

TEST_CASE("mtk::MidiHumanizer only touches notes in scope", "[avnd][midi][humanize]")
{
  SECTION("out-of-range keys pass through untouched")
  {
    driver d;
    auto& in = d.fx.inputs;
    in.timing.value = 0.02f;
    in.early.value = 0.f;
    in.velocity.value = 40;
    in.key_range.value = {60.f, 72.f};
    in.seed.value = 6;

    d.note_on(40, 100, 128); // below the range
    d.tick(8192);
    d.drain();

    const auto ons = d.notes_on();
    REQUIRE(ons.size() == 1);
    CHECK(ons[0].at == 128);
    CHECK(ons[0].velocity == 100);
  }

  SECTION("other channels pass through untouched")
  {
    driver d;
    auto& in = d.fx.inputs;
    in.timing.value = 0.02f;
    in.velocity.value = 40;
    in.channel.value = 1;
    in.seed.value = 6;

    d.note_on(60, 100, 128, /* channel */ 5);
    d.tick(8192);
    d.drain();

    const auto ons = d.notes_on();
    REQUIRE(ons.size() == 1);
    CHECK(ons[0].at == 128);
    CHECK(ons[0].velocity == 100);
  }
}

TEST_CASE("mtk::MidiHumanizer clamps velocity into its range", "[avnd][midi][humanize]")
{
  driver d;
  auto& in = d.fx.inputs;
  in.timing.value = 0.f;
  in.velocity.value = 64; // huge deviation
  in.velocity_range.value = {40.f, 80.f};
  in.seed.value = 12;

  for(int i = 0; i < 200; i++)
  {
    d.note_on(60, 100, 0);
    d.note_off(60, 200);
    d.tick(1024);
  }
  d.drain();

  const auto ons = d.notes_on();
  REQUIRE(ons.size() > 190);
  bool saw_variation = false;
  for(auto& e : ons)
  {
    CHECK(e.velocity >= 40);
    CHECK(e.velocity <= 80);
    if(e.velocity != ons[0].velocity)
      saw_variation = true;
  }
  CHECK(saw_variation);
}

TEST_CASE("mtk::MidiHumanizer never hangs a note", "[avnd][midi][humanize]")
{
  // Every path that throws away the pending queue has to turn off whatever it
  // has already sent out, otherwise the note sounds forever.
  const auto held = [](const driver& d) {
    int n = 0;
    for(const auto& e : d.out)
    {
      if(e.type == (int)libremidi::message_type::NOTE_ON)
        n++;
      else if(e.type == (int)libremidi::message_type::NOTE_OFF)
        n--;
    }
    return n;
  };

  SECTION("changing the seed mid-note")
  {
    driver d;
    auto& in = d.fx.inputs;
    in.timing.value = 0.02f;
    in.early.value = 1.f; // ~60 ms of latency, so notes sit in the queue
    in.seed.value = 1;

    d.note_on(60, 100, 0);
    d.tick(1024);
    d.note_off(60, 0);
    d.fx.inputs.seed.value = 99; // used to drop the queued note-off
    d.tick(1024);
    d.drain();

    CHECK(held(d) == 0);
  }

  SECTION("all-notes-off while notes are queued")
  {
    driver d;
    auto& in = d.fx.inputs;
    in.timing.value = 0.02f;
    in.early.value = 1.f;
    in.seed.value = 2;

    d.note_on(60, 100, 0);
    d.note_on(64, 100, 0);
    d.note_on(67, 100, 0);

    auto cc = libremidi::channel_events::control_change(1, 123, 0);
    cc.timestamp = 8;
    d.fx.inputs.midi.push_back(cc);

    d.tick(64); // shorter than the latency: the note-ons are still queued
    d.drain();

    CHECK(held(d) == 0);
  }

  SECTION("more overlapping notes than the tracking table holds")
  {
    driver d;
    auto& in = d.fx.inputs;
    in.timing.value = 0.02f;
    in.early.value = 1.f;
    in.seed.value = 3;

    for(int i = 0; i < 100; i++)
      d.note_on(20 + i, 100, 0);
    d.tick(4096);

    for(int i = 0; i < 100; i++)
      d.note_off(20 + i, 0);
    d.tick(4096);
    d.drain();

    // Past the tracking and queue limits, notes may be stolen or dropped -- but
    // never left sounding, which is the only failure that cannot be recovered
    // from. An unmatched note-off is inert.
    CHECK(held(d) <= 0);

    // and no individual pitch is left on
    std::map<int, int> per_pitch;
    for(const auto& e : d.out)
    {
      if(e.type == (int)libremidi::message_type::NOTE_ON)
        per_pitch[e.pitch]++;
      else if(e.type == (int)libremidi::message_type::NOTE_OFF)
        per_pitch[e.pitch]--;
    }
    for(const auto& [pitch, n] : per_pitch)
    {
      INFO("pitch " << pitch);
      CHECK(n <= 0);
    }
  }
}

TEST_CASE("mtk::MidiHumanizer is reproducible for a given seed", "[avnd][midi][humanize]")
{
  const auto run = [](int seed) {
    driver d;
    auto& in = d.fx.inputs;
    in.timing.value = 0.02f;
    in.early.value = 1.f;
    in.velocity.value = 20;
    in.length.value = 0.3f;
    in.seed.value = seed;

    for(int i = 0; i < 40; i++)
    {
      d.note_on(60, 100, 0);
      d.note_off(60, 400);
      d.tick(8192);
    }
    d.drain();
    return d.out;
  };

  const auto a = run(1234);
  const auto b = run(1234);
  const auto c = run(4321);

  REQUIRE(a.size() == b.size());
  for(std::size_t i = 0; i < a.size(); i++)
  {
    CHECK(a[i].at == b[i].at);
    CHECK(a[i].velocity == b[i].velocity);
  }

  bool differs = false;
  for(std::size_t i = 0; i < std::min(a.size(), c.size()); i++)
    if(a[i].at != c[i].at || a[i].velocity != c[i].velocity)
      differs = true;
  CHECK(differs);
}
