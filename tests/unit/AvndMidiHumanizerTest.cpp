#include <Advanced/MidiScaler/MidiHumanizer.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>

#include <array>
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

TEST_CASE("mtk::MidiHumanizer with seed 0 plays a new take each time", "[avnd][midi][humanize]")
{
  driver d;
  auto& in = d.fx.inputs;
  in.timing.value = 0.02f;
  in.early.value = 1.f;
  in.velocity.value = 20;
  in.seed.value = 0;

  const auto play = [&] {
    const int64_t start = d.now;
    const std::size_t first = d.out.size();
    for(int i = 0; i < 40; i++)
    {
      d.note_on(60, 100, 0);
      d.note_off(60, 400);
      d.tick(8192);
    }
    d.drain();
    std::vector<event> take(d.out.begin() + first, d.out.end());
    for(auto& e : take)
      e.at -= start;
    return take;
  };

  const auto a = play();
  d.fx.stop();
  d.tick(0);
  const auto b = play();

  REQUIRE(a.size() == b.size());
  bool differs = false;
  for(std::size_t i = 0; i < a.size(); i++)
    if(a[i].at != b[i].at || a[i].velocity != b[i].velocity)
      differs = true;
  CHECK(differs);
}

namespace
{
//! Per pitch, note-ons and note-offs alternate, starting with a note-on, and
//! nothing is left sounding at the end: no ghost note-off, no double note-on.
void check_well_formed(const std::vector<event>& out)
{
  std::map<int, bool> sounding;
  int ghosts = 0, doubles = 0;
  for(auto& e : out)
  {
    if(e.type == (int)libremidi::message_type::NOTE_ON && e.velocity > 0)
    {
      doubles += sounding[e.pitch];
      sounding[e.pitch] = true;
    }
    else if(
        e.type == (int)libremidi::message_type::NOTE_OFF
        || (e.type == (int)libremidi::message_type::NOTE_ON && e.velocity == 0))
    {
      ghosts += !sounding[e.pitch];
      sounding[e.pitch] = false;
    }
  }
  int hung = 0;
  for(auto& [p, on] : sounding)
    hung += on;
  CHECK(doubles == 0);
  CHECK(ghosts == 0);
  CHECK(hung == 0);
}
}

// A session-player-like stream: repeated and overlapping notes of the same
// pitch, legato, chords. The per-note deviations reorder them; the output
// must still be valid MIDI.
TEST_CASE("mtk::MidiHumanizer always outputs well-formed MIDI", "[avnd][midi][humanize]")
{
  for(int pitchDev : {0, 3})
  {
    CAPTURE(pitchDev);
    driver d;
    auto& in = d.fx.inputs;
    in.timing.value = 0.05f;
    in.early.value = 0.5f;
    in.length.value = 1.f;
    in.velocity.value = 20;
    in.seed.value = 7;
    in.pitch.value = pitchDev;

    uint32_t lcg = 12345;
    auto rnd = [&](int n) {
      lcg = lcg * 1664525u + 1013904223u;
      return int((lcg >> 8) % uint32_t(n));
    };
    std::map<int, int> held; // input-side balance, to keep the input valid
    for(int block = 0; block < 400; block++)
    {
      const int events = rnd(6);
      for(int i = 0; i < events; i++)
      {
        const int pitch = 58 + rnd(6); // few pitches: lots of overlap
        const int ts = rnd(512);
        if(held[pitch] > 0 && rnd(2))
        {
          d.note_off(pitch, ts);
          held[pitch]--;
        }
        else if(held[pitch] < 2) // overlapping instances of the same pitch
        {
          d.note_on(pitch, 90, ts);
          held[pitch]++;
        }
      }
      d.tick(512);
    }
    for(auto& [p, n] : held)
      for(int i = 0; i < n; i++)
        d.note_off(p, 0);
    d.drain(16, 4096);

    REQUIRE(!d.notes_on().empty());
    check_well_formed(d.out);
  }
}

TEST_CASE("mtk::MidiHumanizer transposes notes with Pitch", "[avnd][midi][humanize]")
{
  driver d;
  d.fx.inputs.pitch.value = 4;
  d.fx.inputs.seed.value = 3;
  for(int i = 0; i < 64; i++)
  {
    d.note_on(60, 100, 0);
    d.note_off(60, 100);
    d.tick(512);
  }
  d.drain();
  int moved = 0;
  for(auto& e : d.notes_on())
    moved += e.pitch != 60;
  CHECK(moved > 16);
  check_well_formed(d.out);
}


// ---------------------------------------------------------------------------
// Chance units, velocity as probability, euclid gate, CC randomization, stop

namespace
{
//! A seeded take exercising every control that existed before the chance,
//! gate and CC randomization controls: its output is pinned below.
std::vector<std::array<int64_t, 4>> seeded_reference_take(mtk::MidiHumanizer& fx)
{
  fx.prepare(halp::setup{
      .input_channels = 0, .output_channels = 0, .frames = 512, .rate = 48000.});
  auto& in = fx.inputs;
  in.timing.value = 0.01f;
  in.colour.value = 1.f;
  in.early.value = 0.5f;
  in.velocity.value = 12;
  in.velocity_range.value = {10.f, 120.f};
  in.length.value = 0.3f;
  in.chance.value = 0.7f;
  in.chord_window.value = 0.01f;
  in.spread.value = 0.005f;
  in.pitch.value = 2;
  in.seed.value = 1234;

  std::vector<std::array<int64_t, 4>> out;
  const int block = 512;
  int64_t now = 0;
  for(int b = 0; b < 400; b++)
  {
    const int64_t start = now;
    auto push = [&](libremidi::message m, int64_t at) {
      if(at >= start && at < start + block)
      {
        m.timestamp = at - start;
        in.midi.push_back(m);
      }
    };
    for(int n = 0; n < 48; n++)
    {
      const int64_t on = int64_t(n) * 4000 + (n % 3) * 37;
      const int64_t off = on + 1500 + (n % 5) * 900;
      const int pitch = 48 + (n * 7) % 30;
      const int chord = (n % 4 == 0) ? 3 : 1;
      for(int k = 0; k < chord; k++)
      {
        push(libremidi::channel_events::note_on(1, pitch + k * 4, 40 + (n * 13) % 80), on + k * 20);
        push(libremidi::channel_events::note_off(1, pitch + k * 4, 0), off + k * 20);
      }
      if(n % 6 == 0)
        push(libremidi::channel_events::control_change(1, 1, n), on + 10);
    }
    fx.outputs.midi.midi_messages.clear();
    fx(halp::tick_musical{.frames = block, .tempo = 120., .signature = {4, 4}, .position_in_frames = now});
    for(const auto& m : fx.outputs.midi.midi_messages)
      out.push_back({now + m.timestamp, int64_t(m.bytes[0]),
                     m.size() > 1 ? int64_t(m.bytes[1]) : 0,
                     m.size() > 2 ? int64_t(m.bytes[2]) : 0});
    in.midi.midi_messages.clear();
    now += block;
  }
  return out;
}


//! Output of seeded_reference_take before the chance unit, euclid gate and CC
//! controls existed: at their defaults they must not change a seeded take.
const std::vector<std::array<int64_t, 4>> reference_take_output{
    {730,176,1,0},
    {731,144,54,39},
    {991,144,59,35},
    {2146,128,54,0},
    {2916,128,59,0},
    {4595,144,56,51},
    {6144,128,56,0},
    {8074,144,65,57},
    {11264,128,65,0},
    {12169,144,73,81},
    {15872,128,73,0},
    {16141,144,80,96},
    {16401,144,82,100},
    {16661,144,85,92},
    {20992,128,80,0},
    {20992,128,82,0},
    {21278,128,85,0},
    {24730,176,1,6},
    {24817,144,64,120},
    {26831,128,64,0},
    {28479,144,73,51},
    {31232,128,73,0},
    {33157,144,81,64},
    {33417,144,84,81},
    {36397,128,84,0},
    {36569,128,81,0},
    {36642,144,50,88},
    {40960,128,50,0},
    {41028,144,59,97},
    {42264,128,59,0},
    {48730,176,1,12},
    {48847,144,72,120},
    {49107,144,76,120},
    {49367,144,85,120},
    {51200,128,72,0},
    {51200,128,76,0},
    {51200,128,85,0},
    {64595,144,71,108},
    {65115,144,78,102},
    {66638,128,71,0},
    {67637,128,78,0},
    {69161,144,78,101},
    {71582,128,78,0},
    {72730,176,1,18},
    {73269,144,55,116},
    {75776,128,55,0},
    {80404,144,71,57},
    {81408,128,71,0},
    {84971,144,73,80},
    {87004,128,73,0},
    {92729,144,57,93},
    {96730,176,1,24},
    {96741,128,57,0},
    {97376,144,67,99},
    {101384,144,72,32},
    {102547,128,72,0},
    {103150,128,67,0},
    {105311,144,47,51},
    {107206,128,47,0},
    {109160,144,57,50},
    {112513,128,57,0},
    {113523,144,64,69},
    {113783,144,69,74},
    {117123,128,64,0},
    {117586,144,70,89},
    {117863,128,69,0},
    {120730,176,1,30},
    {120832,128,70,0},
    {125259,144,54,28},
    {127568,128,54,0},
    {129091,144,61,37},
    {129611,144,67,24},
    {131072,128,61,0},
    {131919,128,67,0},
    {136924,144,74,73},
    {141264,144,55,60},
    {142862,128,55,0},
    {143228,128,74,0},
    {144730,176,1,36},
    {145056,144,60,83},
    {145316,144,65,81},
    {145576,144,69,87},
    {147394,128,65,0},
    {147576,128,60,0},
    {148075,128,69,0},
    {149172,144,66,32},
    {152430,128,66,0},
    {157164,144,49,64},
    {160890,144,57,73},
    {161150,144,59,96},
    {161866,128,49,0},
    {162419,128,59,0},
    {162436,128,57,0},
    {168730,176,1,42},
    {176215,144,52,58},
    {176735,144,61,69},
    {180736,128,61,0},
    {180821,144,60,68},
    {181883,128,52,0},
    {182083,128,60,0},
};

//! Drives the object with a musical position: quarters_per_block quarter
//! notes go by in each block.
struct musical_driver : driver
{
  double quarters{};

  void tick_musical(int frames, double quarters_per_block)
  {
    fx.outputs.midi.midi_messages.clear();
    fx(halp::tick_musical{
        .frames = frames,
        .tempo = 120.,
        .signature = {4, 4},
        .position_in_frames = now,
        .start_position_in_quarters = quarters,
        .end_position_in_quarters = quarters + quarters_per_block});
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
    quarters += quarters_per_block;
  }
};

void check_balanced(const std::vector<event>& out)
{
  std::map<int, int> held;
  for(auto& e : out)
  {
    const bool on = e.type == (int)libremidi::message_type::NOTE_ON;
    if(on && e.velocity > 0)
      REQUIRE(++held[e.pitch] == 1);
    else if(on || e.type == (int)libremidi::message_type::NOTE_OFF)
      REQUIRE(--held[e.pitch] == 0);
  }
  for(auto& [p, n] : held)
    REQUIRE(n == 0);
}
}

TEST_CASE("mtk::MidiHumanizer keeps seeded takes at the new controls' defaults", "[avnd][midi][humanize]")
{
  mtk::MidiHumanizer fx;
  const auto take = seeded_reference_take(fx);
  REQUIRE(take.size() == reference_take_output.size());
  for(std::size_t i = 0; i < take.size(); i++)
  {
    INFO("event " << i);
    REQUIRE(take[i] == reference_take_output[i]);
  }
}

TEST_CASE("mtk::MidiHumanizer chance per chord keeps or drops chords whole", "[avnd][midi][humanize]")
{
  driver d;
  d.fx.inputs.chance.value = 0.5f;
  d.fx.inputs.chance_unit.value = Hum::Chord;
  d.fx.inputs.chord_window.value = 0.01f;
  d.fx.inputs.seed.value = 9;

  int kept = 0;
  for(int i = 0; i < 100; i++)
  {
    d.out.clear();
    d.note_on(60, 100, 0);
    d.note_on(64, 100, 10);
    d.note_on(67, 100, 20);
    d.note_off(60, 2000);
    d.note_off(64, 2000);
    d.note_off(67, 2000);
    d.tick(8192);
    const auto n = d.notes_on().size();
    REQUIRE((n == 0 || n == 3));
    REQUIRE(d.notes_off().size() == n);
    kept += n == 3;
  }
  CHECK(kept > 30);
  CHECK(kept < 70);
}

TEST_CASE("mtk::MidiHumanizer timed chance decides once per period", "[avnd][midi][humanize]")
{
  // 0.5 s at 120 bpm: one quarter note per period, four notes in each
  musical_driver d;
  d.fx.inputs.chance.value = 0.5f;
  d.fx.inputs.chance_unit.value = Hum::Period;
  d.fx.inputs.chance_period.value = 0.5f;
  d.fx.inputs.seed.value = 5;

  int open = 0;
  for(int p = 0; p < 100; p++)
  {
    d.out.clear();
    for(int k = 0; k < 4; k++)
    {
      d.note_on(60 + k, 100, 10);
      d.note_off(60 + k, 3000);
      d.tick_musical(6000, 0.25);
    }
    const auto n = d.notes_on().size();
    REQUIRE((n == 0 || n == 4));
    REQUIRE(d.notes_off().size() == n);
    open += n == 4;
  }
  CHECK(open > 30);
  CHECK(open < 70);
}

TEST_CASE("mtk::MidiHumanizer velocity as probability", "[avnd][midi][humanize]")
{
  driver d;
  d.fx.inputs.velocity_chance.value = true;
  d.fx.inputs.seed.value = 3;

  int loud = 0, soft = 0;
  for(int i = 0; i < 400; i++)
  {
    d.out.clear();
    d.note_on(60, 127, 0);
    d.note_off(60, 100);
    d.note_on(62, 32, 200);
    d.note_off(62, 300);
    d.tick(4096);
    for(auto& e : d.notes_on())
      (e.pitch == 60 ? loud : soft)++;
    REQUIRE(d.notes_on().size() == d.notes_off().size());
  }
  CHECK(loud == 400);
  CHECK(soft > 60);
  CHECK(soft < 150);
}

TEST_CASE("mtk::MidiHumanizer euclid gate drops notes on rests", "[avnd][midi][humanize]")
{
  // Sixteenth notes: 0.125 s at 120 bpm, one per block of 6000 frames
  const auto run = [](int steps, int pulses, int rotation, bool musical) {
    musical_driver d;
    d.fx.inputs.euclid.value = true;
    d.fx.inputs.euclid_steps.value = steps;
    d.fx.inputs.euclid_pulses.value = pulses;
    d.fx.inputs.euclid_rotation.value = rotation;
    d.fx.inputs.euclid_step.value = 0.125f;
    std::vector<bool> played;
    for(int i = 0; i < 32; i++)
    {
      d.out.clear();
      d.note_on(60, 100, 0);
      d.note_off(60, 1000);
      if(musical)
        d.tick_musical(6000, 0.25);
      else
        d.tick(6000);
      played.push_back(!d.notes_on().empty());
      REQUIRE(d.notes_on().size() == d.notes_off().size());
    }
    return played;
  };

  for(bool musical : {true, false})
  {
    const auto four = run(4, 1, 0, musical);
    for(int i = 0; i < 32; i++)
      CHECK(four[i] == (i % 4 == 0));

    const auto rotated = run(4, 1, 1, musical);
    for(int i = 0; i < 32; i++)
      CHECK(rotated[i] == (i % 4 == 1));

    const auto tresillo = run(8, 3, 0, musical);
    for(int i = 0; i < 32; i++)
      CHECK(tresillo[i] == mtk::euclid_hit(i, 8, 3, 0));
  }

  SECTION("the grid follows the musical position, not the frame count")
  {
    musical_driver d;
    d.fx.inputs.euclid.value = true;
    d.fx.inputs.euclid_steps.value = 2;
    d.fx.inputs.euclid_pulses.value = 1;
    d.fx.inputs.euclid_step.value = 0.5f; // one quarter at 120 bpm
    d.quarters = 1.; // starts on the second step: a rest
    d.note_on(60, 100, 0);
    d.note_off(60, 10);
    d.tick_musical(512, 0.01);
    CHECK(d.notes_on().empty());
  }
}

TEST_CASE("mtk::MidiHumanizer randomizes one controller", "[avnd][midi][humanize]")
{
  const auto run = [](int seed) {
    driver d;
    d.fx.inputs.random_cc.value = 1;
    d.fx.inputs.random_cc_range.value = {-10.f, 5.f};
    d.fx.inputs.seed.value = seed;
    std::vector<int> values;
    for(int i = 0; i < 200; i++)
    {
      d.out.clear();
      auto a = libremidi::channel_events::control_change(1, 1, 64);
      auto b = libremidi::channel_events::control_change(1, 2, 64);
      d.fx.inputs.midi.push_back(a);
      d.fx.inputs.midi.push_back(b);
      d.tick(512);
      REQUIRE(d.out.size() == 2);
      REQUIRE(d.out[0].pitch == 1);
      REQUIRE(d.out[1].pitch == 2);
      REQUIRE(d.out[1].velocity == 64);
      REQUIRE(d.out[0].velocity >= 54);
      REQUIRE(d.out[0].velocity <= 69);
      values.push_back(d.out[0].velocity);
    }
    return values;
  };
  const auto a = run(11);
  CHECK(a == run(11));
  CHECK(a != run(12));
  CHECK(*std::min_element(a.begin(), a.end()) < 57);
  CHECK(*std::max_element(a.begin(), a.end()) > 66);

  SECTION("values stay in 0 - 127")
  {
    driver d;
    d.fx.inputs.random_cc.value = 7;
    d.fx.inputs.random_cc_range.value = {50.f, 60.f};
    d.fx.inputs.midi.push_back(libremidi::channel_events::control_change(1, 7, 100));
    d.tick(512);
    REQUIRE(d.out.size() == 1);
    CHECK(d.out[0].velocity == 127);
  }
}

TEST_CASE("mtk::MidiHumanizer stop ends every note in a 0-frame tick", "[avnd][midi][humanize]")
{
  driver d;
  d.fx.inputs.timing.value = 0.02f;
  d.fx.inputs.early.value = 1.f;
  d.fx.inputs.seed.value = 8;

  // Held notes, and note-ons still queued behind the latency
  d.note_on(60, 100, 0);
  d.note_on(64, 100, 10);
  d.tick(8192);
  d.note_on(67, 100, 8000);
  d.tick(8192);

  const auto sounding = d.notes_on().size();
  REQUIRE(sounding >= 2);

  d.fx.stop();
  d.fx.outputs.midi.midi_messages.clear();
  d.fx(halp::tick_musical{.frames = 0});
  int offs = 0;
  for(auto& m : d.fx.outputs.midi.midi_messages)
  {
    REQUIRE(m.timestamp == 0);
    REQUIRE(!mtk::is_note_on(m));
    offs += mtk::is_note_off(m);
  }
  CHECK(offs == int(sounding));

  // Nothing comes out afterwards, the note-offs of the input included
  d.out.clear();
  d.note_off(60, 0);
  d.note_off(64, 0);
  d.note_off(67, 0);
  d.drain();
  CHECK(d.notes_on().empty());
  CHECK(d.notes_off().empty());
}

TEST_CASE("mtk::MidiHumanizer new controls never hang a note", "[avnd][midi][humanize]")
{
  musical_driver d;
  d.fx.inputs.timing.value = 0.01f;
  d.fx.inputs.early.value = 0.5f;
  d.fx.inputs.seed.value = 21;
  d.fx.inputs.chance.value = 0.6f;
  d.fx.inputs.velocity_chance.value = true;
  d.fx.inputs.euclid.value = true;
  d.fx.inputs.euclid_steps.value = 5;
  d.fx.inputs.euclid_pulses.value = 3;
  d.fx.inputs.euclid_step.value = 0.0625f;

  for(int i = 0; i < 300; i++)
  {
    // Controls change while notes are held
    d.fx.inputs.chance_unit.value = Hum::ChanceUnit(i / 20 % 3);
    d.fx.inputs.euclid.value = (i / 7) % 2;
    d.note_on(40 + i % 40, 1 + (i * 37) % 127, i % 300);
    if(i % 9 == 0)
      d.note_on(40 + i % 40, 0, 400); // velocity-0 note-on is a note-off
    d.note_off(40 + (i + 40 - 3) % 40, 450);
    d.tick_musical(512, 512. / 24000.);
  }
  for(int p = 40; p < 80; p++)
    d.note_off(p, 0);
  d.drain();
  check_balanced(d.out);
}
