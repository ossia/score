// Unit tests for Patternist's step engine.
//
// The node is driven directly: with the musical fields of the token left at
// zero, get_quantification_date() returns prev_date, so every tick advances the
// pattern by exactly one step. That keeps these tests about note pairing rather
// than about the quantization arithmetic, which token_request owns and
// QuantificationTest covers.

#include <Patternist/PatternNode.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

namespace
{
using namespace Patternist;

constexpr int note_on = CMIDI2_STATUS_NOTE_ON;
constexpr int note_off = CMIDI2_STATUS_NOTE_OFF;

struct decoded
{
  int status{};
  int channel{};
  int note{};
  int64_t timestamp{};
};

decoded decode(const libremidi::ump& u)
{
  return {
      cmidi2_ump_get_status_code(u.data), cmidi2_ump_get_channel(u.data),
      cmidi2_ump_get_midi2_note_note(u.data), u.timestamp};
}

Lane lane(uint8_t note, std::initializer_list<Note> steps)
{
  return Lane{std::vector<Note>{steps}, note};
}

struct fixture
{
  pattern_node node;
  ossia::execution_state st; // modelToSamplesRatio == 1
  ossia::midi_port& port{*node.out.target<ossia::midi_port>()};

  fixture()
  {
    node.channel = to_midi_channel(1);
    node.in_flight_channel = node.channel;
  }

  void set(std::vector<Lane> lanes, int length)
  {
    node.pattern.lanes = std::move(lanes);
    node.pattern.length = length;
    node.pattern.division = 16;
  }

  //! Runs one tick, which plays exactly one step.
  std::vector<decoded>
  step(int64_t offset = 0, bool discontinuous = false, double speed = 1.)
  {
    port.messages.clear();

    ossia::exec_state_facade fac{&st};
    ossia::token_request tk{
        ossia::time_value{0},      ossia::time_value{100},
        ossia::time_value{100000}, ossia::time_value{offset},
        speed,                     ossia::time_signature{4, 4},
        120.};
    tk.end_discontinuous = discontinuous;
    static_cast<ossia::graph_node&>(node).run(tk, fac);

    std::vector<decoded> out;
    out.reserve(port.messages.size());
    for(const auto& m : port.messages)
      out.push_back(decode(m));
    return out;
  }
};
}

TEST_CASE("patternist: a step releases what the previous one held", "[midi][pattern]")
{
  fixture f;
  f.set({lane(36, {Note::Note, Note::Rest})}, 2);

  auto s0 = f.step();
  REQUIRE(s0.size() == 1);
  CHECK(s0[0].status == note_on);
  CHECK(s0[0].note == 36);

  auto s1 = f.step();
  REQUIRE(s1.size() == 1);
  CHECK(s1[0].status == note_off);
  CHECK(s1[0].note == 36);
}

TEST_CASE("patternist: notes alternating between two lanes", "[midi][pattern]")
{
  // 36 38 36 38 ... : every step must release the previous lane's note before
  // the new one is struck.
  fixture f;
  f.set(
      {lane(36, {Note::Note, Note::Rest}), lane(38, {Note::Rest, Note::Note})}, 2);

  auto s0 = f.step();
  REQUIRE(s0.size() == 1);
  CHECK(s0[0].status == note_on);
  CHECK(s0[0].note == 36);

  auto s1 = f.step();
  REQUIRE(s1.size() == 2);
  CHECK(s1[0].status == note_off);
  CHECK(s1[0].note == 36);
  CHECK(s1[1].status == note_on);
  CHECK(s1[1].note == 38);

  auto s2 = f.step();
  REQUIRE(s2.size() == 2);
  CHECK(s2[0].status == note_off);
  CHECK(s2[0].note == 38);
  CHECK(s2[1].status == note_on);
  CHECK(s2[1].note == 36);
}

TEST_CASE("patternist: a repeated note is released before being struck", "[midi][pattern]")
{
  fixture f;
  f.set({lane(36, {Note::Note, Note::Note})}, 2);

  REQUIRE(f.step().size() == 1);

  auto s1 = f.step();
  REQUIRE(s1.size() == 2);
  CHECK(s1[0].status == note_off);
  CHECK(s1[1].status == note_on);
  CHECK(s1[0].note == 36);
  CHECK(s1[1].note == 36);
}

TEST_CASE("patternist: legato holds a note across steps", "[midi][pattern]")
{
  // 36 38-38 36: the legato step emits nothing, the note is released only when
  // the lane goes back to Rest.
  fixture f;
  f.set(
      {lane(36, {Note::Note, Note::Rest, Note::Rest, Note::Note}),
       lane(38, {Note::Rest, Note::Note, Note::Legato, Note::Rest})},
      4);

  auto s0 = f.step();
  REQUIRE(s0.size() == 1);
  CHECK(s0[0].note == 36);
  CHECK(s0[0].status == note_on);

  auto s1 = f.step();
  REQUIRE(s1.size() == 2);
  CHECK(s1[0].status == note_off);
  CHECK(s1[0].note == 36);
  CHECK(s1[1].status == note_on);
  CHECK(s1[1].note == 38);

  auto s2 = f.step();
  CHECK(s2.empty()); // held through the legato step

  auto s3 = f.step();
  REQUIRE(s3.size() == 2);
  CHECK(s3[0].status == note_off);
  CHECK(s3[0].note == 38);
  CHECK(s3[1].status == note_on);
  CHECK(s3[1].note == 36);
}

TEST_CASE("patternist: a legato step re-strikes a note that is not held", "[midi][pattern]")
{
  fixture f;
  f.set({lane(36, {Note::Legato, Note::Rest})}, 2);

  auto s0 = f.step();
  REQUIRE(s0.size() == 1);
  CHECK(s0[0].status == note_on);
  CHECK(s0[0].note == 36);

  auto s1 = f.step();
  REQUIRE(s1.size() == 1);
  CHECK(s1[0].status == note_off);
}

TEST_CASE("patternist: end_discontinuous releases inside the tick", "[midi][pattern]")
{
  // Stamping at 0 puts the release before the start of the tick as soon as the
  // interval does not begin on a buffer boundary; consumers that window on
  // [tick_start; tick_start + frames[ drop it, and in_flight is cleared here so
  // the note would never be released again.
  fixture f;
  f.set({lane(36, {Note::Note, Note::Rest})}, 2);

  REQUIRE(f.step().size() == 1);

  auto disc = f.step(/* offset */ 32, /* discontinuous */ true);
  REQUIRE(disc.size() == 1);
  CHECK(disc[0].status == note_off);
  CHECK(disc[0].note == 36);
  CHECK(disc[0].timestamp == 32);

  // and it is not released a second time
  CHECK(f.step().empty());
}

TEST_CASE("patternist: all_notes_off clears the in-flight set", "[midi][pattern]")
{
  fixture f;
  f.set({lane(36, {Note::Note, Note::Rest})}, 2);

  REQUIRE(f.step().size() == 1);

  f.port.messages.clear();
  f.node.all_notes_off();
  REQUIRE(f.port.messages.size() == 1);
  CHECK(decode(f.port.messages[0]).status == note_off);

  // the next step must not release it again
  CHECK(f.step().empty());
}

TEST_CASE("patternist: channels are converted to the wire range", "[midi][pattern]")
{
  CHECK(to_midi_channel(1) == 0);
  CHECK(to_midi_channel(10) == 9);
  CHECK(to_midi_channel(16) == 15);
  CHECK(to_midi_channel(0) == 0);   // clamped
  CHECK(to_midi_channel(99) == 15); // clamped
}

TEST_CASE("patternist: changing channel releases on the old one", "[midi][pattern]")
{
  // The note-on went out on the previous channel: sending its note-off on the
  // new one leaves the synth holding it forever.
  fixture f;
  f.set({lane(36, {Note::Note, Note::Note})}, 2);

  auto s0 = f.step();
  REQUIRE(s0.size() == 1);
  CHECK(s0[0].channel == 0);

  f.node.set_channel(to_midi_channel(10));

  auto s1 = f.step();
  REQUIRE(s1.size() == 2);
  CHECK(s1[0].status == note_off);
  CHECK(s1[0].note == 36);
  CHECK(s1[0].channel == 0); // released on the channel it was struck on
  CHECK(s1[1].status == note_on);
  CHECK(s1[1].channel == 9);
}

TEST_CASE("patternist: a degenerate pattern does not divide by zero", "[midi][pattern]")
{
  fixture f;
  f.set({lane(36, {Note::Note})}, 0);

  CHECK(f.step().empty());
}

TEST_CASE("patternist: steps past the end of a lane release held notes", "[midi][pattern]")
{
  // The pattern is longer than the lane: those steps have no content, but a
  // note held from an earlier one still has to come back off.
  fixture f;
  f.set({lane(36, {Note::Note})}, 4);

  REQUIRE(f.step().size() == 1);

  auto s1 = f.step();
  REQUIRE(s1.size() == 1);
  CHECK(s1[0].status == note_off);
  CHECK(s1[0].note == 36);

  CHECK(f.step().empty());
}

TEST_CASE("patternist: a null speed does not produce a bad timestamp", "[midi][pattern]")
{
  fixture f;
  f.set({lane(36, {Note::Note, Note::Rest})}, 2);

  auto s0 = f.step(/* offset */ 16, /* discontinuous */ false, /* speed */ 0.);
  REQUIRE(s0.size() == 1);
  CHECK(s0[0].timestamp == 16);
}

TEST_CASE("patternist: notes above the MIDI range are ignored", "[midi][pattern]")
{
  // 255 and 254 are the accent and slide lanes, they must not be struck.
  fixture f;
  f.set(
      {lane(255, {Note::Note, Note::Rest}), lane(254, {Note::Note, Note::Rest}),
       lane(36, {Note::Note, Note::Rest})},
      2);

  auto s0 = f.step();
  REQUIRE(s0.size() == 1);
  CHECK(s0[0].note == 36);
}
