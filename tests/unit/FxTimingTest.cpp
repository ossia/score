// Free metronome v2 and Rate Limiter v2: their period / interval is a time
// chooser, in seconds or synced to a note value.

#include <Fx/Types.hpp>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <Fx/Arpeggiator_v2.hpp>
#include <Fx/Metro.hpp>
#include <Fx/Quantifier_v2.hpp>
#include <Fx/Metro_v2.hpp>
#include <Fx/RateLimiter_v2.hpp>

#include <ossia/dataflow/execution_state.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

namespace
{
template <typename Port>
std::unique_ptr<std::vector<int64_t>> capture(Port& port)
{
  auto out = std::make_unique<std::vector<int64_t>>();
  port.call.context = out.get();
  port.call.function = +[](void* ctx, int64_t t) {
    static_cast<std::vector<int64_t>*>(ctx)->push_back(t);
  };
  return out;
}

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
}

TEST_CASE("Free metronome v2: a tick every period of seconds", "[fx][metro]")
{
  Nodes::Metro::v2::Node m;
  m.prepare({.input_channels = 0, .output_channels = 0, .frames = 512, .rate = 48000.});
  auto ticks = capture(m.outputs.out);
  m.inputs.period.value = 0.25f;
  m.inputs.period.sync = false;

  // One second in buffers of 512: ticks fall inside buffers, not only where a
  // buffer starts exactly on a multiple of the period.
  std::vector<int64_t> abs;
  for(int64_t pos = 0; pos < 48000; pos += 512)
  {
    ticks->clear();
    m(musical(pos, 512, pos / 24000., (pos + 512) / 24000., 120.));
    for(auto t : *ticks)
      abs.push_back(pos + t);
  }
  // (the last buffer ends past 48000, the fifth tick)
  CHECK(abs == std::vector<int64_t>{0, 12000, 24000, 36000, 48000});
}

TEST_CASE("Free metronome (old): a tick every period too", "[fx][metro]")
{
  Nodes::Metro::Node m;
  m.prepare({.input_channels = 0, .output_channels = 0, .frames = 512, .rate = 48000.});
  auto ticks = capture(m.outputs.out);
  m.inputs.frequency.value = 4.f; // Hz
  m.inputs.quantify.value = false;
  std::vector<int64_t> abs;
  for(int64_t pos = 0; pos < 48000; pos += 512)
  {
    ticks->clear();
    m(musical(pos, 512, pos / 24000., (pos + 512) / 24000., 120.));
    for(auto t : *ticks)
      abs.push_back(pos + t);
  }
  CHECK(abs == std::vector<int64_t>{0, 12000, 24000, 36000, 48000});
}

TEST_CASE("Free metronome v2: synced, on the quarter notes", "[fx][metro]")
{
  Nodes::Metro::v2::Node m;
  m.prepare({.input_channels = 0, .output_channels = 0, .frames = 512, .rate = 48000.});
  auto ticks = capture(m.outputs.out);
  // A quarter at 90 BPM, as the binding hands it over: seconds, synced.
  m.inputs.period.value = 60.f / 90.f;
  m.inputs.period.sync = true;

  // From quarter 0.5 to quarter 2.5 in one tick of 1000 frames: quarters 1 and 2.
  m(musical(0, 1000, 0.5, 2.5, 90.));
  CHECK(*ticks == std::vector<int64_t>{250, 750});
}

TEST_CASE("Rate Limiter v2: at most one value per interval", "[fx][ratelimiter]")
{
  Nodes::RateLimiter::v2::Node rl;
  ossia::execution_state state;
  state.bufferSize = 1000;
  state.modelToSamplesRatio = 1.;
  state.samplesToModelRatio = 1.;
  rl.ossia_state = {&state};
  ossia::value_port in;
  rl.inputs.port.value = &in;
  std::vector<std::pair<int64_t, ossia::value>> out;
  rl.outputs.out.call.context = &out;
  rl.outputs.out.call.function = +[](void* ctx, int64_t t, ossia::value v) {
    static_cast<decltype(out)*>(ctx)->emplace_back(t, std::move(v));
  };
  rl.inputs.interval.value = 0.5f; // seconds
  rl.inputs.interval.sync = false;

  constexpr int64_t second = 705'600'000;
  auto tick = [&](int64_t from, int64_t to) {
    ossia::token_request t{};
    t.prev_date = ossia::time_value{from};
    t.date = ossia::time_value{to};
    t.speed = 1.;
    in.write_value(1.f, 0);
    rl(t);
    in.get_data().clear();
  };
  // A value every 0.1 s during one second: one goes out every 0.5 s.
  for(int i = 0; i < 10; i++)
    tick(i * second / 10, (i + 1) * second / 10);
  CHECK(out.size() == 2);
}

TEST_CASE("Arpeggiator v2: the rate's note value is the grid's", "[fx][arpeggiator]")
{
  Nodes::Arpeggiator::v2::Node arp;
  halp::tick_musical tk{};
  tk.tempo = 120.;
  // A synced eighth at 120 BPM, as the binding hands it over: 0.25 s.
  arp.inputs.rate.value = 0.25f;
  arp.inputs.rate.sync = true;
  CHECK(arp.grid_rate(tk) == 8.);
  // The same length at another tempo is another note value: a sixteenth
  tk.tempo = 60.;
  CHECK(arp.grid_rate(tk) == 16.);
  arp.inputs.rate.value = 0.f;
  CHECK(arp.grid_rate(tk) == 0.);
}

namespace
{
using QEvents = std::vector<std::tuple<int64_t, int, int>>; // frame, status, pitch

//! 1 kHz, 100-frame ticks; `in` is (frame, pitch, on) at absolute frames.
QEvents quantify(
    float grid, float tightness, Nodes::Quantifier::v2::NoteLength len, float duration,
    std::vector<std::tuple<int64_t, int, bool>> in, int64_t until = 3000)
{
  Nodes::Quantifier::v2::Node q;
  q.prepare({.input_channels = 0, .output_channels = 0, .frames = 100, .rate = 1000.});
  q.inputs.grid.value = grid;
  q.inputs.tightness.value = tightness;
  q.inputs.length.value = len;
  q.inputs.duration.value = duration;
  QEvents events;
  for(int64_t pos = 0; pos < until; pos += 100)
  {
    q.inputs.midi.midi_messages.clear();
    q.outputs.midi.midi_messages.clear();
    for(auto [at, pitch, on] : in)
      if(at >= pos && at < pos + 100)
      {
        auto m = on ? libremidi::channel_events::note_on(1, pitch, 100)
                    : libremidi::channel_events::note_off(1, pitch, 0);
        m.timestamp = at - pos;
        q.inputs.midi.midi_messages.push_back(m);
      }
    halp::tick_flicks tk{};
    tk.frames = 100;
    tk.position_in_frames = pos;
    q(tk);
    for(auto& m : q.outputs.midi.midi_messages)
      events.emplace_back(pos + m.timestamp, int(m.get_message_type()), int(m.bytes[1]));
  }
  return events;
}
int64_t first_frame(const QEvents& e)
{
  REQUIRE(!e.empty());
  return std::get<0>(e.front());
}
constexpr int ON = int(libremidi::message_type::NOTE_ON);
constexpr int OFF = int(libremidi::message_type::NOTE_OFF);
}

TEST_CASE("Midi quantify v2: the three lengths", "[fx][quantifier]")
{
  using enum Nodes::Quantifier::v2::NoteLength;
  // A 0.5 s grid at 1 kHz: played at 630, starts at 1000
  // Fixed duration: 250 ms, whatever the note-off does
  CHECK(
      quantify(0.5f, 1.f, FixedDuration, 0.25f, {{630, 60, true}, {700, 60, false}})
      == QEvents{{1000, ON, 60}, {1250, OFF, 60}});
  // End on grid: on the next 0.4 s point after the start
  CHECK(
      quantify(0.5f, 1.f, EndOnGrid, 0.4f, {{630, 60, true}})
      == QEvents{{1000, ON, 60}, {1200, OFF, 60}});
  // Until the note-off: held past the start, it ends when released
  CHECK(
      quantify(0.5f, 1.f, UntilNoteOff, 0.f, {{630, 60, true}, {1400, 60, false}})
      == QEvents{{1000, ON, 60}, {1400, OFF, 60}});
  // No grid: as it comes
  CHECK(
      quantify(0.f, 1.f, FixedDuration, 0.25f, {{130, 60, true}})
      == QEvents{{130, ON, 60}, {380, OFF, 60}});
}

TEST_CASE("Midi quantify v2: no stuck notes", "[fx][quantifier]")
{
  using enum Nodes::Quantifier::v2::NoteLength;
  // Released before its quantized start: it keeps the length it was played
  // with, and stops.
  CHECK(
      quantify(0.5f, 1.f, UntilNoteOff, 0.f, {{630, 60, true}, {730, 60, false}})
      == QEvents{{1000, ON, 60}, {1100, OFF, 60}});
  // The same key twice: the first one ends before the second starts
  const auto twice
      = quantify(0.f, 1.f, UntilNoteOff, 0.f, {{100, 60, true}, {300, 60, true}, {500, 60, false}});
  CHECK(twice == QEvents{{100, ON, 60}, {300, OFF, 60}, {300, ON, 60}, {500, OFF, 60}});
}

TEST_CASE("Midi quantify v2: tightness lets a note just late through", "[fx][quantifier]")
{
  using enum Nodes::Quantifier::v2::NoteLength;
  // 0.5 s grid: 520 is 20 ms after the point at 500.
  // Tight: waits for 1000
  CHECK(first_frame(quantify(0.5f, 1.f, FixedDuration, 0.1f, {{520, 60, true}})) == 1000);
  // Loose (0.8: up to 50 ms late is on time): now
  CHECK(first_frame(quantify(0.5f, 0.8f, FixedDuration, 0.1f, {{520, 60, true}})) == 520);
  // Too late even for that: the next point
  CHECK(first_frame(quantify(0.5f, 0.8f, FixedDuration, 0.1f, {{600, 60, true}})) == 1000);
}
