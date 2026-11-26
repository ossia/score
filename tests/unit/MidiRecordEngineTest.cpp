// The MIDI recorder of the recording engine, driven tick by tick as the graph
// drives it (see AudioRecordEngineTest for the clock), and the notes a take
// makes:
//  - every message is dated in the box where it arrived, to the sample;
//  - started on the next bar, messages before the bar are not in the take;
//  - waiting for the first message, the take starts on it;
//  - with a length, messages after its end are not in the take;
//  - note ons and offs pair into notes; a note held at the end ends there.

#include <Recording/Engine/MidiRecorder.hpp>
#include <Recording/Takes/RecordedNotes.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/midi_port.hpp>
#include <ossia/dataflow/nodes/forward_node.hpp>

#include <libremidi/reader.hpp>
#include <libremidi/ump_events.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <vector>

using namespace Recording;

namespace
{
constexpr int rate = 48000;
constexpr int buffer = 480;
constexpr int64_t flicks_per_second = 705600000;
constexpr int64_t tick_flicks = flicks_per_second / 100;
constexpr int64_t flicks_per_sample = flicks_per_second / rate;
constexpr double Q = 352800000.0;

struct Rig
{
  ossia::execution_state st;
  std::shared_ptr<ossia::nodes::forward_node> clock
      = std::make_shared<ossia::nodes::forward_node>();
  std::shared_ptr<MidiTake> take = std::make_shared<MidiTake>(4096);
  midi_record_node node{take, clock};
  int64_t boxDate = 0;
  std::vector<MidiTake::Event> events;

  Rig()
  {
    st.sampleRate = rate;
    st.bufferSize = buffer;
    st.modelToSamplesRatio = double(rate) / flicks_per_second;
    st.samplesToModelRatio = flicks_per_second / double(rate);
  }

  //! One buffer with these messages at these samples of it
  void tick(std::vector<std::pair<int, libremidi::ump>> msgs = {})
  {
    auto& in = *node.root_inputs()[0]->target<ossia::midi_port>();
    in.messages.clear();
    for(auto& [at, m] : msgs)
    {
      in.messages.push_back(m);
      in.messages.back().timestamp = at;
    }

    clock->requested_tokens.clear();
    ossia::token_request tk;
    tk.prev_date = ossia::time_value{boxDate};
    tk.date = ossia::time_value{boxDate + tick_flicks};
    tk.speed = 1.;
    tk.tempo = 120.;
    tk.signature = ossia::time_signature{4, 4};
    tk.start_sample = 0;
    tk.length_sample = buffer;
    tk.musical_start_position = boxDate / Q;
    tk.musical_end_position = (boxDate + tick_flicks) / Q;
    tk.musical_start_last_bar = std::floor(tk.musical_start_position / 4.) * 4.;
    tk.musical_end_last_bar = std::floor(tk.musical_end_position / 4.) * 4.;
    clock->requested_tokens.push_back(tk);

    ossia::graph_node& base = node;
    base.run(ossia::token_request{}, ossia::exec_state_facade{&st});
    boxDate += tick_flicks;

    MidiTake::Event ev;
    while(take->queue.try_dequeue(ev))
      events.push_back(ev);
  }
};

libremidi::ump on(int pitch, int vel = 100)
{
  // libremidi channels are 0-based; it gives MIDI 2 messages
  return libremidi::from_midi1::note_on(0, pitch, vel);
}
libremidi::ump off(int pitch)
{
  return libremidi::from_midi1::note_off(0, pitch, 0);
}
}

TEST_CASE("A MIDI message is dated in the box where it arrived", "[recording][midi]")
{
  Rig r;
  r.take->startRequested = true;
  r.tick({{100, on(60)}});
  r.tick();
  r.tick({{240, off(60)}, {241, on(64)}});
  r.take->stopRequested = true;
  r.tick();

  REQUIRE(r.events.size() == 3);
  CHECK(r.events[0].date == 100 * flicks_per_sample);
  CHECK(r.events[1].date == 2 * tick_flicks + 240 * flicks_per_sample);
  CHECK(r.events[2].date == 2 * tick_flicks + 241 * flicks_per_sample);
  CHECK(r.take->startDate == 0);

  const auto notes = notesOf(r.events, r.take->endDate);
  REQUIRE(notes.size() == 2);
  CHECK(notes[0]
        == RecordedNote{100 * flicks_per_sample, 2 * tick_flicks + 140 * flicks_per_sample,
                        60, 100, 0});
  // Held at the end: ends with the take
  CHECK(notes[1].pitch == 64);
  CHECK(notes[1].start + notes[1].duration == r.take->endDate);
}

TEST_CASE("Started on the next bar, earlier messages are not in the take", "[recording][midi]")
{
  Rig r;
  // Asked half a second in (a bar line at the box's start would count as the
  // next one): the bar is at 2 s, tick 200, sample 0 of it
  for(int i = 0; i < 50; i++)
    r.tick();
  r.take->startRate = 1.;
  r.take->startRequested = true;
  for(int i = 50; i < 199; i++)
    r.tick({{10, on(50)}});
  r.tick({{479, on(51)}});
  r.tick({{0, on(52)}, {5, on(53)}});
  REQUIRE(r.events.size() == 2);
  CHECK(r.events[0].date == 200 * tick_flicks);
  CHECK(r.take->startPosition == 4.);
}

TEST_CASE("Waiting for the first message, the take starts on it", "[recording][midi]")
{
  Rig r;
  r.take->startOnFirstEvent = true;
  r.take->startRequested = true;
  for(int i = 0; i < 10; i++)
    r.tick();
  CHECK(r.take->state == MidiTake::Waiting);
  r.tick({{300, on(60)}});
  CHECK(r.take->state == MidiTake::Recording);
  CHECK(r.take->startDate == 10 * tick_flicks + 300 * flicks_per_sample);
  REQUIRE(r.events.size() == 1);
  CHECK(r.events[0].date == r.take->startDate);
}

TEST_CASE("With a length, messages after its end are not in the take", "[recording][midi]")
{
  Rig r;
  r.take->length = 3 * tick_flicks; // 1440 samples
  r.take->startRequested = true;
  r.tick({{0, on(60)}});
  r.tick();
  r.tick({{479, off(60)}});
  r.tick({{0, on(61)}});
  CHECK(r.take->state == MidiTake::Done);
  REQUIRE(r.events.size() == 2);
  CHECK(r.take->endDate == 3 * tick_flicks);
}

TEST_CASE("Notes pair their ons and offs", "[recording][midi]")
{
  std::vector<MidiTake::Event> ev;
  ev.push_back({0, on(60)});
  ev.push_back({10, on(60)});  // the same note again: ends the first
  ev.push_back({15, off(61)}); // nothing to end
  ev.push_back({20, on(62, 0)}); // velocity 0: a note off, nothing to end
  ev.push_back({30, off(60)});
  const auto n = notesOf(ev, 100);
  REQUIRE(n.size() == 2);
  CHECK(n[0] == RecordedNote{0, 10, 60, 100, 0});
  CHECK(n[1] == RecordedNote{10, 20, 60, 100, 0});
}

TEST_CASE("A seek ends the take where it was", "[recording][midi][seek]")
{
  Rig r;
  r.take->startRequested = true;
  r.tick({{10, on(60)}});
  r.tick();
  const int64_t before = r.take->endDate;
  CHECK(r.take->state == MidiTake::Recording);
  // Playback jumps ahead by a second
  r.boxDate += flicks_per_second;
  r.tick({{10, on(62)}});
  CHECK(r.take->state == MidiTake::Done);
  CHECK(r.take->endDate == before);
  // What came after the jump is not in the take
  REQUIRE(r.events.size() == 1);
  CHECK(r.events[0].date < before);

  // Back to the start: the same
  Rig b;
  b.take->startRequested = true;
  b.boxDate = 10 * tick_flicks;
  b.tick();
  b.tick();
  b.boxDate = 0;
  b.tick();
  CHECK(b.take->state == MidiTake::Done);
}

TEST_CASE("A take ends when its box stops playing", "[recording][midi][box-stop]")
{
  Rig r;
  r.take->startRequested = true;
  r.tick({{10, on(60)}});
  r.tick();
  const int64_t end = r.take->endDate;
  CHECK(r.take->state == MidiTake::Recording);
  // Its box does not play any more: no token
  r.clock->requested_tokens.clear();
  ossia::graph_node& base = r.node;
  base.run(ossia::token_request{}, ossia::exec_state_facade{&r.st});
  base.run(ossia::token_request{}, ossia::exec_state_facade{&r.st});
  CHECK(r.take->state == MidiTake::Done);
  CHECK(r.take->endDate == end);
}

TEST_CASE("Quantized notes start on the grid, as long as they were", "[recording][midi][quantize]")
{
  constexpr int64_t q = 352800000; // a quarter note
  std::vector<RecordedNote> notes{
      {q / 10, 1000, 60, 100, 0},        // a little after 0
      {q - q / 10, 2000, 61, 100, 0},    // a little before the first quarter
      {q + q / 2 + 1, 3000, 62, 100, 0}, // just past the half: up
      {q + q / 2 - 1, 4000, 63, 100, 0}, // just before it: down
  };
  auto quarter = notes;
  quantizeNotes(quarter, 1.);
  REQUIRE(quarter.size() == 4);
  // Sorted by where they start now; lengths, pitches, velocities kept
  CHECK(quarter[0] == RecordedNote{0, 1000, 60, 100, 0});
  CHECK(quarter[1] == RecordedNote{q, 2000, 61, 100, 0});
  CHECK(quarter[2] == RecordedNote{q, 4000, 63, 100, 0});
  CHECK(quarter[3] == RecordedNote{2 * q, 3000, 62, 100, 0});

  // Sixteenths
  auto sixteenth = notes;
  quantizeNotes(sixteenth, 0.25);
  for(const auto& n : sixteenth)
    CHECK(n.start % (q / 4) == 0);
  CHECK(sixteenth[0].start == 0);

  // No quantization: as played
  auto none = notes;
  quantizeNotes(none, 0.);
  CHECK(none == notes);
}

TEST_CASE("A MIDI take's file counts quarters at the tempo it was played in", "[recording][midi][file]")
{
  // At 90, a quarter lasts 2/3 s. A take from 10 s, two bars long, with a
  // note on its second quarter lasting a quarter, and one held past its end.
  constexpr int64_t sec = 705600000;
  const int64_t quarter = sec * 2 / 3;
  const int64_t start = 10 * sec;
  const int64_t length = 8 * quarter;
  std::vector<RecordedNote> notes{
      {start + quarter, quarter, 60, 90, 0},
      {start + 7 * quarter, 4 * quarter, 64, 70, 3},
  };
  const auto bytes = smfOf(notes, start, length, 90., 3, 4);
  libremidi::reader r;
  std::vector<uint8_t> raw(bytes.begin(), bytes.end());
  REQUIRE(r.parse(raw) != libremidi::reader::invalid);
  REQUIRE(r.tracks.size() == 1);
  CHECK(r.ticksPerBeat == 480.);
  // The file ends where the take does, whatever its last note
  CHECK(r.get_end_time() == 8. * 480.);

  int tick = 0;
  bool tempo = false, meter = false;
  std::vector<std::array<int, 4>> ons, offs; // tick, pitch, velocity, channel
  for(const auto& ev : r.tracks[0])
  {
    tick += ev.tick;
    if(ev.m.is_meta_event()
       && ev.m.get_meta_event_type() == libremidi::meta_event_type::TEMPO_CHANGE)
    {
      tempo = true;
      CHECK(tick == 0);
      REQUIRE(ev.m.size() == 6);
      CHECK(((ev.m.bytes[3] << 16) | (ev.m.bytes[4] << 8) | ev.m.bytes[5]) == 666667);
    }
    else if(ev.m.is_meta_event()
            && ev.m.get_meta_event_type() == libremidi::meta_event_type::TIME_SIGNATURE)
    {
      meter = true;
      REQUIRE(ev.m.size() == 7);
      CHECK(ev.m.bytes[3] == 3);
      CHECK(ev.m.bytes[4] == 2);
    }
    else if(ev.m.get_message_type() == libremidi::message_type::NOTE_ON && ev.m.bytes[2] > 0)
      ons.push_back({tick, ev.m.bytes[1], ev.m.bytes[2], ev.m.get_channel()});
    else if(ev.m.get_message_type() == libremidi::message_type::NOTE_OFF
            || ev.m.get_message_type() == libremidi::message_type::NOTE_ON)
      offs.push_back({tick, ev.m.bytes[1], 0, ev.m.get_channel()});
  }
  CHECK(tempo);
  CHECK(meter);
  REQUIRE(ons.size() == 2);
  REQUIRE(offs.size() == 2);
  CHECK(ons[0] == std::array<int, 4>{480, 60, 90, 1});
  CHECK(offs[0] == std::array<int, 4>{960, 60, 0, 1});
  // Channels count from 1 in a file
  CHECK(ons[1] == std::array<int, 4>{7 * 480, 64, 70, 4});
  // Cut at the end of the take
  CHECK(offs[1] == std::array<int, 4>{8 * 480, 64, 0, 4});
}
