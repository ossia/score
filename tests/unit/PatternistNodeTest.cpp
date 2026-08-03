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

  //! Runs one rewinding tick, which steps back through exactly one step.
  std::vector<decoded> step_backward()
  {
    port.messages.clear();

    ossia::exec_state_facade fac{&st};
    ossia::token_request tk{
        ossia::time_value{100},    ossia::time_value{0},
        ossia::time_value{100000}, ossia::time_value{0},
        -1.,                       ossia::time_signature{4, 4},
        120.};
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

TEST_CASE("patternist: stopping releases what is held", "[midi][pattern]")
{
  // all_notes_off() runs outside of a tick, and init_outlet() clears every
  // outlet before the node runs again - so writing the note-offs there sends
  // them nowhere. The flag is consumed on the next tick instead, and the
  // process requests one so that tick actually happens.
  fixture f;
  f.set({lane(36, {Note::Note, Note::Rest})}, 2);

  REQUIRE(f.step().size() == 1);

  f.port.messages.clear();
  f.node.all_notes_off();
  CHECK(f.port.messages.empty()); // nothing written outside the tick

  auto msgs = f.step();
  REQUIRE(msgs.size() == 1);
  CHECK(msgs[0].status == note_off);
  CHECK(msgs[0].note == 36);

  // and it is not released a second time
  CHECK(f.step().empty());
}

TEST_CASE("patternist: the stop flag is honoured on an empty tick", "[midi][pattern]")
{
  // What node_process::stop() requests is a default token: prev_date == date,
  // which the step engine otherwise skips.
  fixture f;
  f.set({lane(36, {Note::Note, Note::Rest})}, 2);

  REQUIRE(f.step().size() == 1);

  f.node.mustStop = true;

  f.port.messages.clear();
  ossia::exec_state_facade fac{&f.st};
  static_cast<ossia::graph_node&>(f.node).run(ossia::token_request{}, fac);

  REQUIRE(f.port.messages.size() == 1);
  CHECK(decode(f.port.messages[0]).status == note_off);
  CHECK(decode(f.port.messages[0]).note == 36);
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

TEST_CASE("patternist: a tick covering several steps plays them all", "[midi][pattern]")
{
  // With the single-date version only the first step of the tick was played:
  // a small division, a large buffer or a high tempo were enough to drop the
  // rest, silently.
  fixture f;
  f.set(
      {lane(36, {Note::Note, Note::Rest, Note::Note, Note::Rest}),
       lane(38, {Note::Rest, Note::Note, Note::Rest, Note::Note})},
      4);

  f.port.messages.clear();

  ossia::exec_state_facade fac{&f.st};
  ossia::token_request tk{
      ossia::time_value{0},      ossia::time_value{1000},
      ossia::time_value{100000}, ossia::time_value{0},
      1.,                        ossia::time_signature{4, 4},
      120.};
  // One quarter note of music: four sixteenths, so four steps.
  tk.musical_start_position = 0.;
  tk.musical_end_position = 1.;
  static_cast<ossia::graph_node&>(f.node).run(tk, fac);

  std::vector<decoded> msgs;
  for(const auto& m : f.port.messages)
    msgs.push_back(decode(m));

  // step 0: on 36 | step 1: off 36, on 38 | step 2: off 38, on 36 | step 3: off 36, on 38
  REQUIRE(msgs.size() == 7);
  CHECK(msgs[0].status == note_on);
  CHECK(msgs[0].note == 36);
  CHECK(msgs[0].timestamp == 0);

  CHECK(msgs[1].status == note_off);
  CHECK(msgs[1].note == 36);
  CHECK(msgs[2].status == note_on);
  CHECK(msgs[2].note == 38);
  CHECK(msgs[1].timestamp == 250);
  CHECK(msgs[2].timestamp == 250);

  CHECK(msgs[3].status == note_off);
  CHECK(msgs[3].note == 38);
  CHECK(msgs[4].status == note_on);
  CHECK(msgs[4].note == 36);
  CHECK(msgs[4].timestamp == 500);

  CHECK(msgs[5].status == note_off);
  CHECK(msgs[5].note == 36);
  CHECK(msgs[6].status == note_on);
  CHECK(msgs[6].note == 38);
  CHECK(msgs[6].timestamp == 750);
}

TEST_CASE("patternist: rewinding walks the pattern backwards", "[midi][pattern]")
{
  // The step used to come from a counter that only ever incremented, so a
  // rewinding timeline still marched the sequence forwards.
  fixture f;
  f.set(
      {lane(36, {Note::Note, Note::Rest, Note::Rest, Note::Rest}),
       lane(37, {Note::Rest, Note::Note, Note::Rest, Note::Rest}),
       lane(38, {Note::Rest, Note::Rest, Note::Note, Note::Rest}),
       lane(39, {Note::Rest, Note::Rest, Note::Rest, Note::Note})},
      4);

  const auto struck = [](const std::vector<decoded>& v) {
    int n = -1;
    for(const auto& m : v)
      if(m.status == note_on)
        n = m.note;
    return n;
  };

  CHECK(struck(f.step()) == 36);
  CHECK(struck(f.step()) == 37);
  CHECK(struck(f.step()) == 38);
  CHECK(struck(f.step()) == 39);

  // Rewinding retraces the steps it just played, in reverse.
  CHECK(struck(f.step_backward()) == 39);
  CHECK(struck(f.step_backward()) == 38);
  CHECK(struck(f.step_backward()) == 37);
  CHECK(struck(f.step_backward()) == 36);

  // And going forward again resumes where the rewind left off.
  CHECK(struck(f.step()) == 36);
  CHECK(struck(f.step()) == 37);
}

namespace
{
//! Drives a node over real musical ticks: [prev; date] in model units with the
//! matching musical positions, the way time_interval::tick_impl fills them.
//! 1000 model units per quarter, 4/4.
void musical_tick(fixture& f, int64_t prev_units, int64_t date_units)
{
  f.port.messages.clear();
  ossia::exec_state_facade fac{&f.st};
  ossia::token_request tk{
      ossia::time_value{prev_units},
      ossia::time_value{date_units},
      ossia::time_value{100000},
      ossia::time_value{0},
      date_units >= prev_units ? 1. : -1.,
      ossia::time_signature{4, 4},
      120.};
  tk.musical_start_position = prev_units / 1000.;
  tk.musical_end_position = date_units / 1000.;
  tk.musical_start_last_bar = std::floor(tk.musical_start_position / 4.) * 4.;
  tk.musical_end_last_bar = std::floor(tk.musical_end_position / 4.) * 4.;
  static_cast<ossia::graph_node&>(f.node).run(tk, fac);
}
}

TEST_CASE(
    "patternist: out and back over a real grid returns to the same step",
    "[midi][pattern]")
{
  // Pattern length 3 does not divide the 16 sixteenths of a 4/4 bar, so any
  // bookkeeping that counts per-bar rather than per-crossing drifts.
  fixture f;
  f.set(
      {lane(36, {Note::Note, Note::Rest, Note::Rest}),
       lane(37, {Note::Rest, Note::Note, Note::Rest}),
       lane(38, {Note::Rest, Note::Rest, Note::Note})},
      3);
  f.node.pattern.division = 16;

  SECTION("turnaround exactly on a grid point")
  {
    // Forward 0 -> 6 quarters in ticks of 0.6, back the same way. 6.0 sits on
    // the sixteenth grid: the forward pass leaves it to the next tick, the
    // backward pass owns it, and the crossing counts still agree.
    for(int i = 0; i < 10; i++)
      musical_tick(f, i * 600, (i + 1) * 600);
    const int after_forward = f.node.current;
    // 24 crossings from 0 included to 6.0 excluded
    CHECK(after_forward == 24 % 3);

    for(int i = 10; i > 0; i--)
      musical_tick(f, i * 600, (i - 1) * 600);
    CHECK(f.node.current == 0);
  }

  SECTION("turnaround off the grid")
  {
    // Forward 0 -> 6.1 quarters: 25 crossings (0 through 6.0). Back down to 0:
    // 24 crossings (6.0 through 0.25; 0 was crossed on the way out and is
    // never uncrossed). Net one step forward, which is where a playhead
    // standing at 0 belongs: step 0 already played, step 1 next.
    for(int i = 0; i < 10; i++)
      musical_tick(f, i * 610, (i + 1) * 610);
    CHECK(f.node.current == 25 % 3);

    for(int i = 10; i > 0; i--)
      musical_tick(f, i * 610, (i - 1) * 610);
    CHECK(f.node.current == 1);
  }
}

TEST_CASE(
    "patternist: a rewinding stop stamps its releases inside the tick",
    "[midi][pattern]")
{
  // A note is playing; the interval stops while the timeline rewinds, with a
  // tick offset. The note-off must carry a timestamp inside the tick's window:
  // a negative one is dropped by every consumer that windows on
  // [tick_start; tick_start + frames[, and the note hangs forever.
  fixture f;
  f.set({lane(36, {Note::Note, Note::Rest})}, 2);

  auto s0 = f.step();
  REQUIRE(s0.size() == 1);
  REQUIRE(s0[0].status == note_on);

  f.port.messages.clear();
  ossia::exec_state_facade fac{&f.st};
  ossia::token_request tk{
      ossia::time_value{100},   ossia::time_value{0},
      ossia::time_value{100000}, ossia::time_value{50},
      -1.,                       ossia::time_signature{4, 4},
      120.};
  tk.end_discontinuous = true;
  static_cast<ossia::graph_node&>(f.node).run(tk, fac);

  REQUIRE(f.port.messages.size() == 1);
  auto off = decode(f.port.messages[0]);
  CHECK(off.status == note_off);
  CHECK(off.note == 36);
  CHECK(off.timestamp >= 0);
}
