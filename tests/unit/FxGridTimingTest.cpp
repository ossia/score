// The musical and free grids of the timing objects at every buffer size,
// tempo and sample rate: Free metronome v2 fires each grid point exactly once
// and on its sample, Midi quantify v2 stays on the beats when synced wherever
// playback started, Rate Limiter v2 never loses the last value of a burst and
// sends the latest one on its grid, the arpeggiator takes a note-on of
// velocity 0 as a note-off and expands its repeats in place.

#include <Fx/Arpeggiator_v2.hpp>
#include <Fx/Metro_v2.hpp>
#include <Fx/Quantifier_v2.hpp>
#include <Fx/RateLimiter_v2.hpp>

#include <ossia/dataflow/execution_state.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

namespace
{
halp::tick_musical musical(int64_t pos, int frames, double q0, double q1, double tempo)
{
  halp::tick_musical tk{};
  tk.frames = frames;
  tk.position_in_frames = pos;
  tk.start_position_in_quarters = q0;
  tk.end_position_in_quarters = q1;
  tk.tempo = tempo;
  tk.signature = {4, 4};
  return tk;
}

//! Runs a synced metronome from quarter `q_start` over `seconds`, and returns
//! the absolute frames it fired at.
std::vector<int64_t> run_synced_metro(
    double period_quarters, double tempo, double rate, int buffer, double q_start,
    double seconds)
{
  Nodes::Metro::v2::Node m;
  m.prepare({.input_channels = 0, .output_channels = 0, .frames = buffer, .rate = rate});
  std::vector<int64_t> ticks;
  int64_t pos = 0;
  m.outputs.out.call.context = &ticks;
  m.outputs.out.call.function = +[](void* ctx, int64_t t) {
    static_cast<std::vector<int64_t>*>(ctx)->push_back(t);
  };
  m.inputs.period.value = float(period_quarters * 60. / tempo);
  m.inputs.period.sync = true;

  const double quarters_per_frame = tempo / 60. / rate;
  std::vector<int64_t> abs;
  const auto total = int64_t(seconds * rate);
  for(; pos < total; pos += buffer)
  {
    ticks.clear();
    const double q0 = q_start + pos * quarters_per_frame;
    const double q1 = q_start + (pos + buffer) * quarters_per_frame;
    m(musical(pos, buffer, q0, q1, tempo));
    for(auto t : ticks)
    {
      REQUIRE(t >= 0);
      REQUIRE(t < buffer);
      abs.push_back(pos + t);
    }
  }
  return abs;
}
}

TEST_CASE("Free metronome v2: every grid point once, at every buffer size", "[fx][metro]")
{
  for(double rate : {44100., 48000., 96000.})
    for(int buffer : {1, 7, 64, 256, 1000, 4096})
      for(double tempo : {97.3, 120., 133.7, 180.})
        for(double period : {0.25, 1. / 3., 1.})
        {
          INFO(
              "rate " << rate << " buffer " << buffer << " tempo " << tempo
                      << " period " << period);
          const double q_start = 2.3; // playback started off the grid
          const auto ticks = run_synced_metro(period, tempo, rate, buffer, q_start, 4.);
          const double frames_per_quarter = rate * 60. / tempo;
          // The first grid point at or after the start, and each one after it
          // The node gets the period as a float control, in seconds: the grid
          // is k times that rounded period, not k times the exact one.
          const double step = double(float(period * 60. / tempo)) * tempo / 60.;
          const auto k0 = int64_t(std::ceil(q_start / step - 1e-9));
          const auto total = int64_t(4. * rate) / buffer * buffer
                             + (int64_t(4. * rate) % buffer ? buffer : 0);
          std::vector<int64_t> expected;
          for(int64_t k = k0;; k++)
          {
            const double f = (k * step - q_start) * frames_per_quarter;
            if(f >= total)
              break;
            expected.push_back(int64_t(std::floor(f + 1e-6)));
          }
          CHECK(ticks == expected);
        }
}

TEST_CASE("Free metronome v2: a grid point on a tick boundary goes out once", "[fx][metro]")
{
  // 0.125 s at 133.7 BPM from quarter 0, one-frame ticks: several grid points
  // fall on a tick boundary up to rounding.
  for(double tempo : {97.3, 133.7, 140.})
  {
    INFO("tempo " << tempo);
    const auto ticks = run_synced_metro(0.125 * tempo / 60., tempo, 44100., 1, 0., 4.);
    REQUIRE(ticks.size() == 32);
    for(std::size_t i = 1; i < ticks.size(); i++)
    {
      const auto gap = ticks[i] - ticks[i - 1];
      CHECK(gap >= 5511);
      CHECK(gap <= 5514);
    }
  }
}

TEST_CASE("Free metronome v2: in seconds, independent of the buffer size", "[fx][metro]")
{
  for(int buffer : {1, 13, 512, 2048})
  {
    Nodes::Metro::v2::Node m;
    m.prepare({.input_channels = 0, .output_channels = 0, .frames = buffer, .rate = 44100.});
    std::vector<int64_t> ticks;
    m.outputs.out.call.context = &ticks;
    m.outputs.out.call.function = +[](void* ctx, int64_t t) {
      static_cast<std::vector<int64_t>*>(ctx)->push_back(t);
    };
    m.inputs.period.value = 0.1f;
    m.inputs.period.sync = false;
    std::vector<int64_t> abs;
    for(int64_t pos = 0; pos < 44100; pos += buffer)
    {
      ticks.clear();
      m(musical(pos, buffer, pos / 22050., (pos + buffer) / 22050., 120.));
      for(auto t : ticks)
        abs.push_back(pos + t);
    }
    INFO("buffer " << buffer);
    std::vector<int64_t> expected;
    for(int64_t k = 0; k * 4410 < (44100 + buffer - 1) / buffer * buffer; k++)
      expected.push_back(k * 4410);
    CHECK(abs == expected);
  }
}

namespace
{
using QEvents = std::vector<std::tuple<int64_t, int, int>>; // frame, status, pitch
constexpr int ON = int(libremidi::message_type::NOTE_ON);
constexpr int OFF = int(libremidi::message_type::NOTE_OFF);

//! 1 kHz, 120 BPM (500 frames a quarter), playback starting at `q_start`.
QEvents quantify_synced(
    double q_start, float grid_quarters, std::vector<std::tuple<int64_t, int, bool>> in,
    int buffer = 100)
{
  Nodes::Quantifier::v2::Node q;
  q.prepare({.input_channels = 0, .output_channels = 0, .frames = buffer, .rate = 1000.});
  q.inputs.grid.value = grid_quarters * 0.5f;
  q.inputs.grid.sync = true;
  q.inputs.tightness.value = 1.f;
  q.inputs.length.value = Nodes::Quantifier::v2::FixedDuration;
  q.inputs.duration.value = 0.1f;
  QEvents events;
  for(int64_t pos = 0; pos < 3000; pos += buffer)
  {
    q.inputs.midi.midi_messages.clear();
    q.outputs.midi.midi_messages.clear();
    for(auto [at, pitch, on] : in)
      if(at >= pos && at < pos + buffer)
      {
        auto m = on ? libremidi::channel_events::note_on(1, pitch, 100)
                    : libremidi::channel_events::note_off(1, pitch, 0);
        m.timestamp = at - pos;
        q.inputs.midi.midi_messages.push_back(m);
      }
    halp::tick_flicks tk{};
    tk.frames = buffer;
    tk.position_in_frames = pos;
    tk.tempo = 120.;
    tk.start_position_in_quarters = q_start + pos / 500.;
    tk.end_position_in_quarters = q_start + (pos + buffer) / 500.;
    q(tk);
    for(auto& m : q.outputs.midi.midi_messages)
      events.emplace_back(pos + m.timestamp, int(m.get_message_type()), int(m.bytes[1]));
  }
  return events;
}
}

TEST_CASE("Midi quantify v2: a synced grid stays on the beats", "[fx][quantifier]")
{
  // Playback starts at quarter 1.5: the next beat is 250 frames in, then every
  // 500 frames.
  for(int buffer : {1, 64, 100, 333})
  {
    INFO("buffer " << buffer);
    CHECK(
        quantify_synced(1.5, 1.f, {{10, 60, true}}, buffer)
        == QEvents{{250, ON, 60}, {350, OFF, 60}});
    CHECK(
        quantify_synced(1.5, 1.f, {{400, 62, true}}, buffer)
        == QEvents{{750, ON, 62}, {850, OFF, 62}});
  }
}

TEST_CASE("Midi quantify v2: events come out in time order", "[fx][quantifier]")
{
  // A note starting and one ending in the same tick, the start earlier: the
  // output is sorted, and at equal frames note-offs made first stay first.
  const auto ev = quantify_synced(0., 1.f, {{10, 60, true}, {620, 62, true}});
  for(std::size_t i = 1; i < ev.size(); i++)
    CHECK(std::get<0>(ev[i - 1]) <= std::get<0>(ev[i]));
  CHECK(ev.size() == 4);
}

namespace
{
struct Limiter
{
  Nodes::RateLimiter::v2::Node rl;
  ossia::execution_state state;
  ossia::value_port in;
  std::vector<std::pair<int64_t, float>> out; // absolute frame, value
  int64_t frames_per_tick{};
  int64_t tick_index{};

  static constexpr int64_t second = 705'600'000;

  explicit Limiter(int64_t frames)
      : frames_per_tick{frames}
  {
    // 1 kHz: a frame is a millisecond
    state.bufferSize = frames;
    state.sampleRate = 1000;
    state.modelToSamplesRatio = 1000. / second;
    state.samplesToModelRatio = double(second) / 1000.;
    rl.ossia_state = {&state};
    rl.inputs.port.value = &in;
    rl.outputs.out.call.context = this;
    rl.outputs.out.call.function = +[](void* ctx, int64_t t, ossia::value v) {
      auto& self = *static_cast<Limiter*>(ctx);
      self.out.emplace_back(self.tick_index * self.frames_per_tick + t, ossia::convert<float>(v));
    };
  }

  //! One tick; `values` are (frame in the tick, value).
  void tick(std::vector<std::pair<int64_t, float>> values, double q0 = 0., double q1 = 0.)
  {
    ossia::token_request t{};
    t.prev_date = ossia::time_value{tick_index * frames_per_tick * second / 1000};
    t.date = ossia::time_value{(tick_index + 1) * frames_per_tick * second / 1000};
    t.speed = 1.;
    t.start_sample = 0;
    t.length_sample = int32_t(frames_per_tick);
    t.tempo = 120.;
    t.signature.upper = 4;
    t.signature.lower = 4;
    t.musical_start_position = q0;
    t.musical_end_position = q1;
    t.musical_start_last_bar = std::floor(q0 / 4.) * 4.;
    t.musical_end_last_bar = std::floor(q1 / 4.) * 4.;
    for(auto [f, v] : values)
      in.write_value(v, f);
    rl(t);
    in.get_data().clear();
    tick_index++;
  }
};
}

TEST_CASE("Rate Limiter v2: the last value of a burst goes out", "[fx][ratelimiter]")
{
  for(int64_t buffer : {16, 64, 512})
  {
    INFO("buffer " << buffer);
    Limiter l{buffer};
    l.rl.inputs.interval.value = 0.1f; // 100 frames
    l.rl.inputs.interval.sync = false;
    // A fader moved from 0 to 1 in 1 ms steps over 250 ms, then left alone
    std::vector<std::pair<int64_t, float>> sent;
    for(int64_t f = 0; f <= 250; f++)
      sent.emplace_back(f, f / 250.f);
    for(int64_t t = 0; t < 1000 / buffer; t++)
    {
      std::vector<std::pair<int64_t, float>> in_tick;
      for(auto [f, v] : sent)
        if(f >= t * buffer && f < (t + 1) * buffer)
          in_tick.emplace_back(f - t * buffer, v);
      l.tick(in_tick);
    }
    REQUIRE(!l.out.empty());
    // One value per 100 frames, and the final position arrives
    CHECK(l.out.front() == std::pair<int64_t, float>{0, 0.f});
    for(std::size_t i = 1; i < l.out.size(); i++)
      CHECK(l.out[i].first - l.out[i - 1].first >= 100);
    CHECK(l.out.back().second == 1.f);
    CHECK(l.out.back().first == 300);
  }
}

TEST_CASE("Rate Limiter v2: synced, the latest value on each grid point", "[fx][ratelimiter]")
{
  // 120 BPM at 1 kHz: an eighth note is 250 frames; synced, the binding hands
  // it over as 0.25 s.
  Limiter l{100};
  l.rl.inputs.interval.value = 0.25f;
  l.rl.inputs.interval.sync = true;
  const double quarters_per_frame = 1. / 500.;
  // Values at frames 10, 20, 30 (before the point at 250), then at 260
  const std::vector<std::pair<int64_t, float>> sent{{10, 1.f}, {20, 2.f}, {30, 3.f}, {260, 4.f}};
  for(int64_t t = 0; t < 10; t++)
  {
    std::vector<std::pair<int64_t, float>> in_tick;
    for(auto [f, v] : sent)
      if(f >= t * 100 && f < (t + 1) * 100)
        in_tick.emplace_back(f - t * 100, v);
    l.tick(in_tick, t * 100 * quarters_per_frame, (t + 1) * 100 * quarters_per_frame);
  }
  CHECK(
      l.out
      == std::vector<std::pair<int64_t, float>>{{250, 3.f}, {500, 4.f}});
}

TEST_CASE("Arpeggiator: a note-on of velocity 0 releases the key", "[fx][arpeggiator]")
{
  Nodes::Arpeggiator::v2::Node arp;
  arp.inputs.rate.value = 0.125f;
  arp.inputs.rate.sync = true;
  arp.inputs.octave.value = 1;
  arp.inputs.repeat.value = 1;
  auto tick = [&](std::vector<libremidi::message> in) {
    arp.inputs.midi.midi_messages.assign(in.begin(), in.end());
    arp.outputs.midi.midi_messages.clear();
    arp(musical(0, 512, 0., 0.5, 120.));
  };
  tick({libremidi::channel_events::note_on(1, 60, 100)});
  REQUIRE(arp.engine.notes.size() == 1);
  tick({libremidi::channel_events::note_on(1, 60, 0)});
  CHECK(arp.engine.notes.empty());
  // Nothing left sounding
  int on = 0;
  for(auto& m : arp.outputs.midi.midi_messages)
    on += m.get_message_type() == libremidi::message_type::NOTE_ON;
  CHECK(on == 0);
}

TEST_CASE("Arpeggiator: repeats and octaves, in order", "[fx][arpeggiator]")
{
  Nodes::Arpeggiator::Engine e;
  for(unsigned char n : {60, 64, 67})
    e.notes.insert({n, 100});
  e.previous_arpeggio = 0; // forward
  e.previous_repeat = 3;
  e.previous_octave = 2;
  e.previous_octave_mode = 1; // above
  e.update();
  std::vector<int> pitches;
  for(auto& c : e.arpeggio)
  {
    REQUIRE(c.size() == 1);
    pitches.push_back(c[0].first);
  }
  CHECK(
      pitches
      == std::vector<int>{60, 60, 60, 64, 64, 64, 67, 67, 67,
                          72, 72, 72, 76, 76, 76, 79, 79, 79});
}
