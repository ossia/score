#include <Advanced/MidiScaler/MidiCommon/ChannelAllocator.hpp>
#include <Advanced/MidiScaler/MidiCommon/EventQueue.hpp>
#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>
#include <Advanced/MidiScaler/MidiCommon/NoteTracker.hpp>
#include <Advanced/MidiScaler/MidiCommon/Rhythm.hpp>
#include <Advanced/MidiScaler/MidiCommon/Theory.hpp>

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>
#include <vector>

using namespace mtk;

namespace
{
libremidi::message on(int ch, int p, int v = 100)
{
  return libremidi::channel_events::note_on(ch, p, v);
}
libremidi::message off(int ch, int p)
{
  return libremidi::channel_events::note_off(ch, p, 0);
}
}

TEST_CASE("classify", "[midi_common]")
{
  CHECK(classify(on(1, 60)) == midi_kind::note_on);
  CHECK(classify(on(1, 60, 0)) == midi_kind::note_off);
  CHECK(classify(off(3, 60)) == midi_kind::note_off);
  CHECK(classify(libremidi::channel_events::control_change(1, 7, 3)) == midi_kind::control_change);
  CHECK(classify(libremidi::channel_events::pitch_bend(1, 8192)) == midi_kind::pitch_bend);
  CHECK(classify(libremidi::message{{0xF8}, 0}) == midi_kind::clock);
  CHECK(classify(libremidi::message{{0xF0, 0x7E, 0xF7}, 0}) == midi_kind::sysex);
  CHECK(classify(libremidi::message{{0xF2, 0, 0}, 0}) == midi_kind::song_position);
  CHECK(classify(libremidi::message{}) == midi_kind::other);
  CHECK(channel_of(on(16, 1)) == 16);
  CHECK(channel_of(libremidi::message{{0xF8}, 0}) == 0);

  auto m = on(1, 60);
  set_channel(m, 10);
  CHECK(channel_of(m) == 10);
  CHECK(is_all_notes_off(libremidi::channel_events::control_change(4, 123, 0)));
  CHECK(!is_all_notes_off(libremidi::channel_events::control_change(4, 64, 0)));
}

TEST_CASE("note names", "[midi_common]")
{
  CHECK(parse_note_name("C4")->first == 60);
  CHECK(parse_note_name("C-1")->first == 0);
  CHECK(parse_note_name("G9")->first == 127);
  CHECK(parse_note_name("F#5")->first == 78);
  CHECK(parse_note_name("Bb3")->first == 58);
  CHECK(!parse_note_name("G#9"));
  CHECK(!parse_note_name("H4"));
  CHECK(!parse_note_name("C"));
}

TEST_CASE("range lists", "[midi_common]")
{
  range_list r;
  REQUIRE(!r.parse("1-4, 6, 9-16", 1, 16));
  CHECK(r.count == 3);
  for(int v : {1, 4, 6, 9, 16})
    CHECK(r.contains(v));
  for(int v : {0, 5, 7, 8, 17})
    CHECK(!r.contains(v));

  REQUIRE(!r.parse("C-1-B2, C4, 97", 0, 127, true));
  CHECK(r.contains(0));
  CHECK(r.contains(47));
  CHECK(!r.contains(48));
  CHECK(r.contains(60));
  CHECK(r.contains(97));

  REQUIRE(!r.parse(" 16 - 9 ", 1, 16));
  CHECK(r.contains(12));

  CHECK(!r.parse("", 0, 127));
  CHECK(r.empty());

  CHECK(r.parse("1-", 1, 16).value() == 2);
  CHECK(r.empty());
  CHECK(r.parse("1, x", 1, 16).value() == 3);
  CHECK(r.parse("0", 1, 16).value() == 0);
  CHECK(r.parse("1 2", 1, 16).value() == 2);

  cached_range_list c;
  CHECK(c.update("1-3", 1, 16).contains(2));
  CHECK(c.valid);
  CHECK(!c.update("5", 1, 16).contains(2));
  c.update("oops", 1, 16);
  CHECK(!c.valid);
  CHECK(c.list.empty());
}

TEST_CASE("note tracker pairs every note-on with one note-off", "[midi_common]")
{
  note_tracker<4, 4> t;
  std::vector<libremidi::message> out;
  auto emit = [&](libremidi::message m) { out.push_back(m); };

  // Two inputs sharing an output note: one note-on, one note-off at the end.
  auto& a = t.begin(1, 60, 0, emit);
  t.play(a, 1, 60, 100, 0, emit);
  t.play(a, 1, 64, 100, 0, emit);
  auto& b = t.begin(1, 64, 0, emit);
  t.play(b, 1, 64, 90, 0, emit);
  CHECK(out.size() == 2);

  REQUIRE(t.end(1, 60, 5, 0, emit));
  CHECK(out.size() == 3); // 60 off, 64 still held by b
  CHECK(is_note_off(out.back()));
  CHECK(out.back().bytes[1] == 60);
  CHECK(out.back().timestamp == 5);

  REQUIRE(t.end(1, 64, 6, 0, emit));
  CHECK(out.size() == 4);
  CHECK(out.back().bytes[1] == 64);
  CHECK(!t.end(1, 64, 7, 0, emit));

  // Full: the oldest is released to make room.
  out.clear();
  for(int i = 0; i < 5; i++)
  {
    auto& h = t.begin(2, 40 + i, 0, emit);
    t.play(h, 2, 40 + i, 100, 0, emit);
  }
  CHECK(t.size() == 4);
  CHECK(out.size() == 6);
  CHECK(is_note_off(out[4]));
  CHECK(out[4].bytes[1] == 40);

  out.clear();
  t.end_all(3, emit);
  CHECK(out.size() == 4);
  CHECK(t.size() == 0);
  for(auto& m : out)
    CHECK(is_note_off(m));
}

TEST_CASE("event queue", "[midi_common]")
{
  event_queue<4> q;
  std::vector<libremidi::message> out;
  auto send = [&](libremidi::message m) { out.push_back(m); };

  q.schedule(100, on(1, 1));
  q.schedule(10, on(1, 2));
  q.schedule(100, on(1, 3));
  q.release(0, 64, send);
  REQUIRE(out.size() == 1);
  CHECK(out[0].bytes[1] == 2);
  CHECK(out[0].timestamp == 10);

  out.clear();
  q.release(64, 64, send);
  REQUIRE(out.size() == 2);
  CHECK(out[0].bytes[1] == 1); // same date: scheduling order
  CHECK(out[1].bytes[1] == 3);
  CHECK(out[0].timestamp == 36);

  // Full queue: a note-off evicts the latest non-note-off.
  out.clear();
  for(int i = 0; i < 4; i++)
    q.schedule(200 + i, on(1, 10 + i));
  q.schedule(300, off(1, 99));
  CHECK(q.size() == 4);
  CHECK(q[3].msg.bytes[1] == 99);
  q.schedule(150, on(1, 50)); // dropped, the queue is full
  CHECK(q.size() == 4);

  q.flush_note_offs(7, send);
  REQUIRE(out.size() == 1);
  CHECK(out[0].bytes[1] == 99);
  CHECK(out[0].timestamp == 7);
  CHECK(q.empty());

  // Late events go out at offset 0.
  out.clear();
  q.schedule(5, on(1, 1));
  q.release(1000, 16, send);
  REQUIRE(out.size() == 1);
  CHECK(out[0].timestamp == 0);

  CHECK(quarters_to_frames(1., 120., 48000.) == 24000);
}

TEST_CASE("channel allocator", "[midi_common]")
{
  channel_allocator a;
  a.set_channels(0b1110); // channels 2, 3, 4
  CHECK(*a.allocate(0) == 2);
  CHECK(*a.allocate(0) == 3);
  CHECK(*a.allocate(0) == 4);
  CHECK(!a.allocate(0));
  a.release(3, 10);
  CHECK(*a.allocate(10) == 3);

  a.reset();
  a.set_release_reserve(100);
  CHECK(*a.allocate(0) == 2);
  a.release(2, 0);
  CHECK(*a.allocate(50) == 3);
  CHECK(*a.allocate(60) == 4);
  CHECK(!a.allocate(99)); // 2 is still in its release reserve
  CHECK(*a.allocate(100) == 2);

  a.reset();
  a.set_release_reserve(0);
  a.set_mode(channel_allocator::mode::ping_pong);
  std::vector<int> seq;
  for(int i = 0; i < 6; i++)
  {
    int c = *a.allocate(i);
    seq.push_back(c);
    a.release(c, i);
  }
  CHECK(seq == std::vector<int>{2, 3, 4, 3, 2, 3});

  a.reset();
  a.set_mode(channel_allocator::mode::lowest);
  CHECK(*a.allocate(0) == 2);
  a.release(2, 0);
  CHECK(*a.allocate(0) == 2);
}

TEST_CASE("euclid and swing", "[midi_common]")
{
  std::string s;
  for(int i = 0; i < 8; i++)
    s += euclid_hit(i, 8, 3, 0) ? 'x' : '.';
  CHECK(s == "x..x..x.");
  CHECK(euclid_hit(1, 8, 3, 1));
  CHECK(!euclid_hit(0, 8, 0, 0));
  CHECK(euclid_hit(5, 8, 9, 0));

  CHECK(swing_delay(0, 0.5) == 0.);
  CHECK(swing_delay(1, 0.) == 0.);
  CHECK(swing_delay(1, 0.5) == 1. / 3.);
  CHECK(swing_delay(3, 2.) == 2. / 3.);
}

TEST_CASE("theory", "[midi_common]")
{
  std::set<std::string_view> names;
  for(const auto& c : chord_types)
  {
    CHECK(names.insert(c.name).second);
    REQUIRE(c.count >= 2);
    CHECK(c.intervals[0] == 0);
    for(int i = 1; i < c.count; i++)
      CHECK(c.intervals[i] > c.intervals[i - 1]);
  }
  CHECK(chord_type_count == 37);
  REQUIRE(find_chord_type("m7b9b5"));
  CHECK(find_chord_type("m7b9b5")->notes().size() == 5);
  CHECK(!find_chord_type("2"));

  CHECK(degree_to_semitones(mode::ionian, 0) == 0);
  CHECK(degree_to_semitones(mode::ionian, 2) == 4);
  CHECK(degree_to_semitones(mode::aeolian, 2) == 3);
  CHECK(degree_to_semitones(mode::ionian, 7) == 12);
  CHECK(degree_to_semitones(mode::ionian, 9) == 16);
  CHECK(degree_to_semitones(mode::ionian, -1) == -1);
  CHECK(degree_to_semitones(mode::ionian, -7) == -12);
  CHECK(degree_to_semitones(mode::harmonic_minor, 6) == 11);

  CHECK(semitones_to_degree(mode::ionian, 4) == 2);
  CHECK(semitones_to_degree(mode::ionian, 6) == 3); // F# -> F, ties go down
  CHECK(semitones_to_degree(mode::ionian, 12) == 7);
  CHECK(semitones_to_degree(mode::ionian, -1) == -1);
  for(int d = -14; d < 14; d++)
    CHECK(semitones_to_degree(mode::dorian, degree_to_semitones(mode::dorian, d)) == d);
}
