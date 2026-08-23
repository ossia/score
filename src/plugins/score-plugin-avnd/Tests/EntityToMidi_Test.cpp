// Unit tests for Entity To MIDI: the invariants of §8.1 of the design
// (stuck notes, duplicate (channel,pitch), off-for-every-on across expiry /
// stop / transport / panic), the adversarial cases of §14 (flicker, voice
// exhaustion, convergence on one quantised pitch, budget saturation), and
// the MIDI byte stream's basic well-formedness.
#include <ossia/network/value/value.hpp>

#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/binding/ossia/to_value.hpp>
#include <avnd/wrappers/controls.hpp>

#include <AvndProcesses/EntityToMidi.hpp>
#include <AvndProcesses/PointTracker.hpp>

#include <catch2/catch_all.hpp>

#include <map>
#include <random>
#include <string>
#include <vector>

using avnd_tools::EntityToMidi;
using TR = EntityToMidi::track_record;
using avnd_tools::E2MCoords;

namespace
{
TR rec(
    int id, float x, float y, const char* state = "confirmed", float conf = 0.9f,
    float age = 1.f, float vx = 0.f, float vy = 0.f)
{
  TR r;
  r.id = id;
  r.slot = id % 8;
  r.state = state;
  r.creation_time = 0.;
  r.age = age;
  r.time_since_seen = 0.f;
  r.position = {x, y, 0.f};
  r.position_raw = {x, y, 0.f};
  r.velocity = {vx, vy, 0.f};
  r.confidence = conf;
  r.provisional = (std::string_view(state) == "provisional");
  r.reacquired = false;
  return r;
}

struct logged_msg
{
  int64_t t; // absolute sample time
  std::vector<uint8_t> bytes;
};

// Parses the emitted stream, asserting well-formedness as it goes and
// tracking outstanding notes per (channel, pitch).
struct StreamCheck
{
  std::map<std::pair<int, int>, int> held;
  int ons = 0, offs = 0;

  static int expected_size(uint8_t status)
  {
    switch(status & 0xF0)
    {
      case 0x80:
      case 0x90:
      case 0xA0:
      case 0xB0:
      case 0xE0:
        return 3;
      case 0xC0:
      case 0xD0:
        return 2;
      default:
        return -1;
    }
  }

  void feed(const logged_msg& m)
  {
    REQUIRE(!m.bytes.empty());
    const uint8_t st = m.bytes[0];
    REQUIRE(st >= 0x80);
    REQUIRE(int(m.bytes.size()) == expected_size(st));
    for(std::size_t i = 1; i < m.bytes.size(); i++)
      REQUIRE(m.bytes[i] < 0x80);

    const int ch = st & 0x0F;
    if((st & 0xF0) == 0x90 && m.bytes[2] > 0)
    {
      auto& h = held[{ch, m.bytes[1]}];
      // Invariant 2: never on/on for the same (channel, pitch).
      INFO("note-on ch=" << (ch + 1) << " pitch=" << int(m.bytes[1]) << " vel=" << int(m.bytes[2]) << " t=" << m.t << " ons=" << ons << " offs=" << offs);
      REQUIRE(h == 0);
      h = 1;
      ons++;
    }
    else if((st & 0xF0) == 0x80 || ((st & 0xF0) == 0x90 && m.bytes[2] == 0))
    {
      auto& h = held[{ch, m.bytes[1]}];
      REQUIRE(h == 1);
      h = 0;
      offs++;
    }
  }

  int outstanding() const
  {
    int n = 0;
    for(auto& [k, v] : held)
      n += v;
    return n;
  }
};

struct Harness
{
  EntityToMidi p;
  double rate = 48000.;
  int bs = 512;
  double tempo = 120.;
  bool advance_transport = true;

  double now_s = 0.;
  double quarters = 0.;
  int64_t sample_pos = 0;
  std::vector<logged_msg> log;
  StreamCheck chk;

  explicit Harness(bool send_config = false)
  {
    // The binding initialises every control from its range().init; a raw
    // struct instance does not, so do it here or every combobox would sit
    // on enumerator 0 instead of its declared default.
    avnd::init_controls(p);
    // Most tests don't want the 80-message MPE configuration burst in the
    // way; the config test enables it explicitly.
    p.inputs.send_config.value = send_config;
    p.prepare({.input_channels = 0, .output_channels = 0, .frames = bs, .rate = rate});
    p.start();
  }

  //! Deliberately routed through ossia::value rather than assigned straight to
  //! the port. That round trip is what a cable does, and skipping it was how a
  //! whole class of decoding bugs stayed invisible: the process is fed records
  //! it can only ever receive as encoded values.
  void send(std::vector<TR> v)
  {
    ossia::value w;
    oscr::to_ossia_value_impl{w}(v);
    if(auto l = w.target<std::vector<ossia::value>>())
      p.inputs.tracks.value = *l;
    else
      p.inputs.tracks.value.clear();
    p.tracks_dirty = true;
  }

  //! Send whatever a user might actually cable in - bare positions, sub-lists,
  //! partial maps - without going through track_record at all.
  void send_raw(std::vector<ossia::value> v)
  {
    p.inputs.tracks.value = std::move(v);
    p.tracks_dirty = true;
  }

  void tick()
  {
    halp::tick_musical tk{};
    tk.frames = bs;
    tk.tempo = tempo;
    tk.signature = {4, 4};
    tk.position_in_frames = sample_pos;
    tk.position_in_nanoseconds = now_s * 1e9;
    tk.start_position_in_quarters = quarters;
    const double dt = bs / rate;
    const double q_end = advance_transport ? quarters + dt * tempo / 60. : quarters;
    tk.end_position_in_quarters = q_end;

    p.outputs.midi.midi_messages.clear(); // the binding does this each tick
    p(tk);

    // Within a tick the wire order must match the timestamp order, or a
    // note-off scheduled later in the buffer can be overtaken by a note-on
    // emitted earlier in the list.
    int64_t prev_ts = -1;
    for(auto& m : p.outputs.midi.midi_messages)
    {
      REQUIRE(m.timestamp >= 0);
      REQUIRE(m.timestamp < bs);
      REQUIRE(m.timestamp >= prev_ts);
      prev_ts = m.timestamp;
      logged_msg lm{sample_pos + m.timestamp, {m.bytes.begin(), m.bytes.end()}};
      chk.feed(lm);
      log.push_back(std::move(lm));
    }

    std::string err;
    if(!p.check_invariants(&err))
      FAIL("invariant violated: " << err);
    // The wire's view and the process's view must agree.
    REQUIRE(chk.outstanding() == p.outstanding());

    sample_pos += bs;
    now_s += dt;
    quarters = q_end;
  }

  void run_ms(double ms)
  {
    const int n = int(ms * 1e-3 * rate / bs) + 1;
    for(int i = 0; i < n; i++)
      tick();
  }

  int count_status(uint8_t status_nibble) const
  {
    int n = 0;
    for(auto& m : log)
      if((m.bytes[0] & 0xF0) == status_nibble)
        n++;
    return n;
  }
};
}

TEST_CASE("sustained: one note per entity lifetime, with expression", "[entity_to_midi]")
{
  Harness h;
  h.p.inputs.scale.value = avnd_tools::E2MScale::None;

  // Entity lives ~1 s, moving continuously with varying speed (constant
  // speed would - correctly - be deadbanded to a single pressure message).
  for(int i = 0; i < 90; i++)
  {
    const float t = i / 90.f;
    const float sp = 0.2f + 1.5f * std::abs(std::sin(6.28f * t));
    h.send({rec(1, 0.2f + 0.5f * t, 0.3f + 0.4f * t, "confirmed", 0.9f, t, sp, 0.f)});
    h.tick();
  }
  REQUIRE(h.chk.ons == 1);
  REQUIRE(h.chk.offs == 0);
  REQUIRE(h.p.held_count() == 1);

  // Expression flows: pitch bend and pressure were emitted.
  REQUIRE(h.count_status(0xE0) > 2);
  REQUIRE(h.count_status(0xD0) > 2);

  // Entity leaves; after the grace period the note is released.
  h.send({});
  h.run_ms(400); // grace 250 ms default
  REQUIRE(h.chk.ons == 1);
  REQUIRE(h.chk.offs == 1);
  REQUIRE(h.chk.outstanding() == 0);
  REQUIRE(h.p.held_count() == 0);
}

TEST_CASE("no note spam from an entity hovering on a scale boundary", "[entity_to_midi]")
{
  Harness h;
  h.p.inputs.scale.value = avnd_tools::E2MScale::Chromatic;

  // Dither exactly across a semitone boundary for 2 seconds.
  for(int i = 0; i < 180; i++)
  {
    const float y = 0.5f + ((i % 2) ? 0.004f : -0.004f);
    h.send({rec(1, 0.5f, y)});
    h.tick();
  }
  // The whole point of the design: still exactly one note.
  REQUIRE(h.chk.ons == 1);
  REQUIRE(h.chk.offs == 0);
}

TEST_CASE("quantiser hysteresis holds the scale step against dithering", "[entity_to_midi]")
{
  Harness h;
  h.p.inputs.scale.value = avnd_tools::E2MScale::Chromatic;
  h.p.inputs.glide.value = 0.f; // no slew: bend == quantised target

  // Position dithering +-0.004 around y=0.5 maps (48..84 range, inverted) to
  // +-0.14 semitone around 66 - inside the default 0.15-step hysteresis.
  std::vector<int> bends;
  for(int i = 0; i < 120; i++)
  {
    const float y = 0.5f + ((i % 2) ? 0.004f : -0.004f);
    h.send({rec(7, 0.5f, y)});
    h.tick();
  }
  for(auto& m : h.log)
    if((m.bytes[0] & 0xF0) == 0xE0)
      bends.push_back(m.bytes[1] | (m.bytes[2] << 7));
  REQUIRE(!bends.empty());
  // The quantised target never moves, so every bend is the initial one.
  for(int b : bends)
    REQUIRE(b == bends.front());
}

TEST_CASE("flicker at one-frame period stays one note; loss past grace releases",
          "[entity_to_midi]")
{
  Harness h;
  // Entity present every other update for 1.5 s - the holes are far shorter
  // than the 250 ms grace.
  for(int i = 0; i < 140; i++)
  {
    if(i % 2)
      h.send({rec(3, 0.4f, 0.6f)});
    else
      h.send({});
    h.tick();
  }
  REQUIRE(h.chk.ons == 1);
  REQUIRE(h.chk.offs == 0);

  // Gone for good.
  h.send({});
  h.run_ms(400);
  REQUIRE(h.chk.offs == 1);
}

TEST_CASE("revive within grace resumes the same note on the same channel",
          "[entity_to_midi]")
{
  Harness h;
  h.send({rec(5, 0.5f, 0.5f)});
  h.run_ms(100);
  REQUIRE(h.chk.ons == 1);
  const auto first_on = std::find_if(h.log.begin(), h.log.end(), [](auto& m) {
    return (m.bytes[0] & 0xF0) == 0x90;
  });
  const int chan = first_on->bytes[0] & 0x0F;
  const int note = first_on->bytes[1];

  // Vanish for 150 ms (inside the 250 ms grace), then return.
  h.send({});
  h.run_ms(150);
  h.send({rec(5, 0.5f, 0.5f, "confirmed", 0.9f, 2.f)});
  h.run_ms(200);

  // No new note-on, nothing released: the note simply continued.
  REQUIRE(h.chk.ons == 1);
  REQUIRE(h.chk.offs == 0);
  REQUIRE(h.p.held_count() == 1);
  (void)chan;
  (void)note;
}

TEST_CASE("watchdog releases notes when tracking data stops entirely",
          "[entity_to_midi]")
{
  Harness h;
  h.send({rec(1, 0.5f, 0.5f)});
  h.run_ms(100);
  REQUIRE(h.p.held_count() == 1);

  // No updates at all - not even an empty list. The tracker is dead.
  // The default 1000 ms watchdog must fire.
  h.run_ms(1500);
  REQUIRE(h.chk.offs == h.chk.ons);
  REQUIRE(h.p.held_count() == 0);
}

TEST_CASE("voice exhaustion: 20 simultaneous entities, denials counted",
          "[entity_to_midi]")
{
  Harness h;
  h.p.inputs.max_voices.value = 8;
  h.p.inputs.allow_steal.value = false;

  std::vector<TR> v;
  for(int i = 1; i <= 20; i++)
    v.push_back(rec(i, i / 21.f, 0.5f, "confirmed", 0.9f, 1.f));
  h.send(v);
  h.run_ms(200);

  REQUIRE(h.p.held_count() == 8);
  REQUIRE(h.chk.ons == 8);
  REQUIRE(h.p.outputs.denied.value > 0);
}

TEST_CASE("stealing: higher-priority newcomer takes the weakest voice, off before on",
          "[entity_to_midi]")
{
  Harness h;
  h.p.inputs.max_voices.value = 2;
  h.p.inputs.priority.value = avnd_tools::E2MPriority::ConfidenceAge;

  // Two low-priority entities...
  h.send({rec(1, 0.3f, 0.5f, "confirmed", 0.4f, 0.2f),
          rec(2, 0.7f, 0.5f, "confirmed", 0.4f, 0.2f)});
  h.run_ms(100);
  REQUIRE(h.chk.ons == 2);

  // ...then a strong newcomer.
  h.send({rec(1, 0.3f, 0.5f, "confirmed", 0.4f, 0.2f),
          rec(2, 0.7f, 0.5f, "confirmed", 0.4f, 0.2f),
          rec(3, 0.5f, 0.2f, "confirmed", 1.f, 3.f)});
  h.run_ms(100);

  REQUIRE(h.chk.ons == 3);
  REQUIRE(h.chk.offs == 1); // the stolen voice was properly released
  REQUIRE(h.p.held_count() == 2);
  // StreamCheck::feed already proved the off preceded the on: a duplicate
  // (channel,pitch) or an on for a busy channel would have failed there.
}

TEST_CASE("a provisional newcomer never steals a confirmed voice", "[entity_to_midi]")
{
  Harness h;
  h.p.inputs.max_voices.value = 1;
  h.p.inputs.trigger_on.value = avnd_tools::E2MTriggerOn::FirstDetection;

  h.send({rec(1, 0.3f, 0.5f, "confirmed", 0.9f, 2.f)});
  h.run_ms(100);
  REQUIRE(h.chk.ons == 1);

  h.send({rec(1, 0.3f, 0.5f, "confirmed", 0.9f, 2.f),
          rec(2, 0.7f, 0.5f, "provisional", 1.f, 5.f)});
  h.run_ms(100);
  REQUIRE(h.chk.ons == 1); // still just the confirmed one
  REQUIRE(h.p.held_count() == 1);
}

TEST_CASE("single channel: two entities on one quantised pitch never double-on",
          "[entity_to_midi]")
{
  Harness h;
  h.p.inputs.output_mode.value = avnd_tools::E2MOutputMode::SingleChannel;
  h.p.inputs.scale.value = avnd_tools::E2MScale::MinorPentatonic;

  // Two entities at exactly the same height: same quantised pitch.
  h.send({rec(1, 0.3f, 0.5f), rec(2, 0.7f, 0.5f)});
  h.run_ms(300);

  // StreamCheck enforces invariant 2 on every message; additionally only one
  // of the two may sound.
  REQUIRE(h.chk.ons == 1);
  REQUIRE(h.p.outputs.denied.value >= 1);

  // The first entity leaves; the second's note can now sound.
  h.send({rec(2, 0.7f, 0.5f)});
  h.run_ms(500);
  REQUIRE(h.chk.ons == 2);
}

TEST_CASE("MPE: each entity gets its own member channel, never the master",
          "[entity_to_midi]")
{
  Harness h;
  std::vector<TR> v;
  for(int i = 1; i <= 5; i++)
    v.push_back(rec(i, i / 6.f, i / 6.f));
  h.send(v);
  h.run_ms(100);

  std::set<int> channels;
  for(auto& m : h.log)
    if((m.bytes[0] & 0xF0) == 0x90)
      channels.insert(m.bytes[0] & 0x0F);
  REQUIRE(channels.size() == 5);
  REQUIRE(channels.count(0) == 0); // lower-zone master is channel 1 (index 0)
}

TEST_CASE("MPE configuration: RPN 6 on the master, RPN 0 per member, spaced",
          "[entity_to_midi]")
{
  Harness h{/*send_config=*/true};
  h.p.inputs.member_channels.value = 3;
  h.run_ms(300); // let the whole spaced burst drain

  // Find CC101/100/6 sequences.
  struct rpn_evt
  {
    int64_t t;
    int ch, cc, val;
  };
  std::vector<rpn_evt> ccs;
  for(auto& m : h.log)
    if((m.bytes[0] & 0xF0) == 0xB0)
      ccs.push_back({m.t, m.bytes[0] & 0x0F, m.bytes[1], m.bytes[2]});
  REQUIRE(!ccs.empty());

  // First three CCs: RPN 6 = 3 members on the master (channel index 0).
  REQUIRE(ccs[0].ch == 0);
  REQUIRE(ccs[0].cc == 101);
  REQUIRE(ccs[0].val == 0);
  REQUIRE(ccs[1].cc == 100);
  REQUIRE(ccs[1].val == 6);
  REQUIRE(ccs[2].cc == 6);
  REQUIRE(ccs[2].val == 3);

  // Bend range 48 went to each member channel (indices 1..3).
  int bend_range_sets = 0;
  for(std::size_t i = 0; i + 2 < ccs.size(); i++)
    if(ccs[i].cc == 101 && ccs[i].val == 0 && ccs[i + 1].cc == 100
       && ccs[i + 1].val == 0 && ccs[i + 2].cc == 6)
    {
      REQUIRE(ccs[i + 2].val == 48);
      REQUIRE(ccs[i].ch >= 1);
      REQUIRE(ccs[i].ch <= 3);
      bend_range_sets++;
    }
  REQUIRE(bend_range_sets == 3);

  // Messages are spaced ~1 ms (48 samples at 48 kHz) apart.
  for(std::size_t i = 1; i < ccs.size(); i++)
    REQUIRE(ccs[i].t - ccs[i - 1].t >= 40);
}

TEST_CASE("panic: off per voice, then CC 120/123/121 per touched channel, spaced",
          "[entity_to_midi]")
{
  Harness h;
  h.send({rec(1, 0.3f, 0.4f), rec(2, 0.7f, 0.6f)});
  h.run_ms(100);
  REQUIRE(h.p.held_count() == 2);

  h.p.panic_requested = true;
  h.run_ms(300);

  REQUIRE(h.chk.outstanding() == 0);
  REQUIRE(h.p.held_count() == 0);
  REQUIRE(h.chk.offs == 2);

  // CC 120, 123, 121 went to every touched channel.
  std::map<int, std::set<int>> ccs_per_chan;
  for(auto& m : h.log)
    if((m.bytes[0] & 0xF0) == 0xB0)
      ccs_per_chan[m.bytes[0] & 0x0F].insert(m.bytes[1]);
  int full = 0;
  for(auto& [ch, ccs] : ccs_per_chan)
    if(ccs.count(120) && ccs.count(123) && ccs.count(121))
      full++;
  REQUIRE(full >= 2);
}

TEST_CASE("transport jump releases and re-acquires", "[entity_to_midi]")
{
  Harness h;
  h.send({rec(1, 0.5f, 0.5f)});
  h.run_ms(100);
  REQUIRE(h.p.held_count() == 1);

  h.p.transport(int64_t(0));
  h.run_ms(100);
  REQUIRE(h.chk.offs >= 1); // the old note was released

  // The entity is still there: the next track update re-acquires.
  h.send({rec(1, 0.5f, 0.5f)});
  h.run_ms(300);
  REQUIRE(h.p.held_count() == 1);
  REQUIRE(h.chk.outstanding() == 1);
}

TEST_CASE("stop() sends a note-off for every held note through the direct path",
          "[entity_to_midi]")
{
  Harness h;
  h.send({rec(1, 0.3f, 0.4f), rec(2, 0.6f, 0.7f), rec(3, 0.8f, 0.2f)});
  h.run_ms(100);
  REQUIRE(h.p.held_count() == 3);

  h.p.stop();
  REQUIRE(h.p.held_count() == 0);
  REQUIRE(h.p.outstanding() == 0);

  int direct_offs = 0, sound_off = 0;
  for(auto& b : h.p.last_direct_panic)
  {
    if((b[0] & 0xF0) == 0x80)
      direct_offs++;
    if((b[0] & 0xF0) == 0xB0 && b[1] == 120)
      sound_off++;
  }
  REQUIRE(direct_offs == 3);
  REQUIRE(sound_off >= 3);
}

TEST_CASE("min note length delays the off of a blink", "[entity_to_midi]")
{
  Harness h;
  h.p.inputs.lost_grace.value = 0.f;
  h.p.inputs.min_note.value = 200.f;

  h.send({rec(1, 0.5f, 0.5f)});
  h.tick();
  REQUIRE(h.chk.ons == 1);
  const int64_t t_on = std::find_if(h.log.begin(), h.log.end(), [](auto& m) {
                         return (m.bytes[0] & 0xF0) == 0x90;
                       })->t;

  h.send({});
  h.run_ms(500);
  REQUIRE(h.chk.offs == 1);
  const int64_t t_off = std::find_if(h.log.begin(), h.log.end(), [](auto& m) {
                          return (m.bytes[0] & 0xF0) == 0x80;
                        })->t;
  REQUIRE(t_off - t_on >= int64_t(0.19 * 48000));
}

TEST_CASE("beat quantisation: onsets land on the eighth-note grid", "[entity_to_midi]")
{
  Harness h;
  h.p.inputs.quant_mode.value = avnd_tools::E2MQuantTargets::Onsets;
  h.p.inputs.grid.value = avnd_tools::E2MGrid::Eighth;
  h.p.inputs.strength.value = 1.f;

  // Advance into the LATTER half of an eighth-note division, so the nearest
  // grid point is ahead and the note must be held. (In the earlier half the
  // nearest grid point is behind, and playing immediately - late - is the
  // correct behaviour: you cannot quantise backwards.)
  h.run_ms(150); // -> 160 ms = 0.32 quarters; next grid point at 0.5 q
  const int64_t trigger_at = h.sample_pos;
  h.send({rec(1, 0.5f, 0.5f)});
  h.run_ms(400);

  REQUIRE(h.chk.ons == 1);
  const auto on = std::find_if(h.log.begin(), h.log.end(), [](auto& m) {
    return (m.bytes[0] & 0xF0) == 0x90;
  });
  // At 120 BPM an eighth is 0.25 s = 12000 samples. The on was held until
  // the next grid point, and sits within one tick (512 samples) of it.
  const double grid_samples = 12000.;
  REQUIRE(on->t > trigger_at);
  const double phase = std::fmod(double(on->t), grid_samples);
  const double dist = std::min(phase, grid_samples - phase);
  REQUIRE(dist <= 512.);
}

TEST_CASE("beat quantisation: transport stall falls back to max hold",
          "[entity_to_midi]")
{
  Harness h;
  h.p.inputs.quant_mode.value = avnd_tools::E2MQuantTargets::Onsets;
  h.p.inputs.max_hold.value = 250.f;
  h.advance_transport = false; // musical position frozen: grid never arrives
  h.run_ms(20);

  h.send({rec(1, 0.5f, 0.5f)});
  h.run_ms(600);
  // The note played anyway, held at most ~max_hold.
  REQUIRE(h.chk.ons == 1);
}

TEST_CASE("triggered mode: fixed-length notes, machine-gunning bounded by lockout",
          "[entity_to_midi]")
{
  Harness h;
  h.p.inputs.note_model.value = avnd_tools::E2MNoteModel::Triggered;
  h.p.inputs.trigger_duration.value = 100.f;
  h.p.inputs.retrig_lockout.value = 120.f;
  h.p.inputs.pitch_tracking.value = avnd_tools::E2MPitchTracking::Retrigger;
  h.p.inputs.scale.value = avnd_tools::E2MScale::Chromatic;

  // An entity sweeping fast across the whole pitch range for 1 s: without
  // the lockout this would fire dozens of notes.
  for(int i = 0; i < 90; i++)
  {
    const float y = (i % 20) / 20.f; // sawtooth sweep
    h.send({rec(1, 0.5f, y)});
    h.tick();
  }
  h.send({});
  h.run_ms(500);

  REQUIRE(h.chk.ons >= 2); // it did retrigger
  // 1 s of sweep + 120 ms lockout: at most ~9 notes, plus slack.
  REQUIRE(h.chk.ons <= 12);
  REQUIRE(h.chk.outstanding() == 0);
  REQUIRE(h.chk.offs == h.chk.ons);
}

TEST_CASE("channel reuse is LRU with a release reserve", "[entity_to_midi]")
{
  Harness h;
  h.p.inputs.lost_grace.value = 0.f;
  h.p.inputs.min_note.value = 0.f;

  h.send({rec(1, 0.5f, 0.5f)});
  h.run_ms(50);
  const auto on1 = std::find_if(h.log.begin(), h.log.end(), [](auto& m) {
    return (m.bytes[0] & 0xF0) == 0x90;
  });
  const int chan1 = on1->bytes[0] & 0x0F;

  h.send({});
  h.run_ms(50);
  REQUIRE(h.chk.offs == 1);

  // A new entity right away: must get a different channel, the freed one is
  // inside its 500 ms release reserve.
  h.send({rec(2, 0.4f, 0.4f)});
  h.run_ms(50);
  REQUIRE(h.chk.ons == 2);
  const auto on2 = std::find_if(h.log.rbegin(), h.log.rend(), [](auto& m) {
    return (m.bytes[0] & 0xF0) == 0x90;
  });
  REQUIRE((on2->bytes[0] & 0x0F) != chan1);
}

TEST_CASE("entry-speed velocity uses the pre-roll peak", "[entity_to_midi]")
{
  // A fast entrance and a slow one must produce clearly different velocities.
  auto entrance_velocity = [](float speed) {
    Harness h;
    // Provisional sightings first (carrying the approach speed), then the
    // confirmation with the tracker's smoothed (lower) instantaneous speed.
    h.send({rec(9, 0.5f, 0.5f, "provisional", 0.9f, 0.02f, speed, 0.f)});
    h.tick();
    h.send({rec(9, 0.5f, 0.5f, "provisional", 0.9f, 0.05f, speed, 0.f)});
    h.tick();
    h.send({rec(9, 0.5f, 0.5f, "confirmed", 0.9f, 0.1f, speed * 0.2f, 0.f)});
    h.run_ms(50);
    for(auto& m : h.log)
      if((m.bytes[0] & 0xF0) == 0x90)
        return int(m.bytes[2]);
    return -1;
  };

  const int fast = entrance_velocity(1.9f);
  const int slow = entrance_velocity(0.05f);
  REQUIRE(fast > 0);
  REQUIRE(slow > 0);
  REQUIRE(fast >= 100); // near max velocity at ~speed_ref
  REQUIRE(slow <= 50);
  REQUIRE(fast - slow > 40);
}

TEST_CASE("2D and 3D tracker records both decode into the shared record type",
          "[entity_to_midi]")
{
  // The 2D tracker emits vec2f positions; the shared type has xyz. The
  // ossia::value round-trip must land x/y and zero z.
  avnd_tools::PointTrackerBase<2>::track_record r2;
  r2.id = 42;
  r2.slot = 3;
  r2.state = "confirmed";
  r2.creation_time = 1.5;
  r2.age = 2.5f;
  r2.time_since_seen = 0.1f;
  r2.position = {0.25f, 0.75f};
  r2.position_raw = {0.26f, 0.74f};
  r2.velocity = {1.f, -1.f};
  r2.confidence = 0.8f;
  r2.provisional = false;
  r2.reacquired = true;

  const ossia::value v = oscr::to_ossia_value(r2);
  TR out;
  REQUIRE(oscr::from_ossia_value(v, out));
  REQUIRE(out.id == 42);
  REQUIRE(out.slot == 3);
  REQUIRE(out.state == "confirmed");
  REQUIRE(out.age == Catch::Approx(2.5));
  REQUIRE(out.position.x == Catch::Approx(0.25));
  REQUIRE(out.position.y == Catch::Approx(0.75));
  REQUIRE(out.position.z == Catch::Approx(0.));
  REQUIRE(out.velocity.x == Catch::Approx(1.));
  REQUIRE(out.confidence == Catch::Approx(0.8));
  REQUIRE(out.provisional == false);
  REQUIRE(out.reacquired == true);

  // 3D round-trips exactly.
  avnd_tools::PointTrackerBase<3>::track_record r3 = out;
  r3.position = {0.1f, 0.2f, 0.3f};
  const ossia::value v3 = oscr::to_ossia_value(r3);
  TR out3;
  REQUIRE(oscr::from_ossia_value(v3, out3));
  REQUIRE(out3.position.z == Catch::Approx(0.3));

  // A whole track list round-trips, as sent over a cable.
  std::vector<avnd_tools::PointTrackerBase<2>::track_record> list{r2, r2};
  const ossia::value vl = oscr::to_ossia_value(list);
  std::vector<TR> out_list;
  REQUIRE(oscr::from_ossia_value(vl, out_list));
  REQUIRE(out_list.size() == 2);
  REQUIRE(out_list[1].id == 42);
}

// ---------------------------------------------------------------------------
// Preset validation on simulated data.
//
// Structural validation (JSON parses, index exists, type and range are right)
// only proves a preset LOADS. It says nothing about whether it produces usable
// MIDI. These cases apply each shipped .scp to the real process, run a
// realistic scene of entities entering, moving, crossing and leaving, and
// assert the properties that must hold for any preset worth shipping: it makes
// sound, it finishes everything it starts, and every byte is legal.
//
// The preset is applied through avendish's own reflection (for_nth over the
// flattened inputs), so this also checks the index table the .scp files encode
// against the real port order rather than against a hand-written list.
// ---------------------------------------------------------------------------
#include <avnd/introspection/input.hpp>

#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>

namespace
{
#if !defined(E2M_PRESET_DIR)
#define E2M_PRESET_DIR \
  "C:/Users/jcelerier/Documents/ossia/score/packages/default/Presets/Entity To MIDI"
#endif

struct PresetEntry
{
  int index;
  double value;
};

struct LoadedPreset
{
  std::string name;
  std::vector<PresetEntry> entries;
};

std::optional<LoadedPreset> read_preset(const std::filesystem::path& p)
{
  std::ifstream f(p, std::ios::binary);
  if(!f)
    return std::nullopt;
  std::stringstream ss;
  ss << f.rdbuf();
  const std::string s = ss.str();

  LoadedPreset out;
  if(auto q = s.find("\"Name\":\""); q != std::string::npos)
  {
    q += 8;
    out.name = s.substr(q, s.find('"', q) - q);
  }

  auto pos = s.find("\"Preset\":[");
  if(pos == std::string::npos)
    return std::nullopt;
  while((pos = s.find('[', pos + 1)) != std::string::npos)
  {
    int idx = 0;
    if(std::sscanf(s.c_str() + pos, "[%d,{", &idx) != 1)
      continue;
    const auto brace = s.find('{', pos);
    if(brace == std::string::npos)
      break;
    const auto colon = s.find(':', brace);
    const auto end = s.find('}', brace);
    if(colon == std::string::npos || end == std::string::npos)
      break;
    const std::string val = s.substr(colon + 1, end - colon - 1);
    double v = 0.;
    if(val.find("true") != std::string::npos)
      v = 1.;
    else if(val.find("false") != std::string::npos)
      v = 0.;
    else
      v = std::atof(val.c_str());
    out.entries.push_back({idx, v});
    pos = end;
  }
  return out;
}

//! Apply one [index, value] pair through avendish reflection. Returns false if
//! the index does not name a settable control - itself a preset bug, since a
//! stale index would be silently ignored and the preset would look fine while
//! doing nothing.
bool apply_entry(EntityToMidi& p, const PresetEntry& e)
{
  bool applied = false;
  const int n = int(avnd::input_introspection<EntityToMidi>::size);
  if(e.index < 0 || e.index >= n)
    return false;
  avnd::input_introspection<EntityToMidi>::for_nth(p.inputs, e.index, [&](auto& field) {
    // Reaching the lambda at all means the index resolves to a real port,
    // which is what "not stale" means. Only some port types carry a value we
    // can set - an impulse such as Panic does not, and score still saves one.
    applied = true;
    if constexpr(requires { field.value; })
    {
      using V = std::decay_t<decltype(field.value)>;
      if constexpr(std::is_enum_v<V>)
        field.value = static_cast<V>(int(e.value));
      else if constexpr(std::is_same_v<V, bool>)
        field.value = e.value != 0.;
      else if constexpr(std::is_arithmetic_v<V>)
        field.value = static_cast<V>(e.value);
    }
  });
  return applied;
}

//! A scene with enough variety that silence means the preset is broken: three
//! entities enter staggered, sweep the field at different speeds, two of them
//! cross, and the third oscillates fast enough to exercise expression.
void run_scene(Harness& h, double seconds = 4.0)
{
  const int frames = int(seconds * 60.);
  for(int i = 0; i < frames; i++)
  {
    const float t = float(i) / 60.f;
    std::vector<TR> v;

    if(t > 0.1f)
    {
      const float u = std::clamp((t - 0.1f) / 3.0f, 0.f, 1.f);
      v.push_back(rec(
          1, 0.05f + 0.9f * u, 0.05f + 0.9f * u, "confirmed", 0.95f, t - 0.1f, 0.3f,
          0.3f));
    }
    if(t > 0.5f)
    {
      const float u = std::clamp((t - 0.5f) / 2.5f, 0.f, 1.f);
      v.push_back(rec(
          2, 0.95f - 0.9f * u, 0.05f + 0.9f * u, "confirmed", 0.85f, t - 0.5f, -0.36f,
          0.36f));
    }
    if(t > 1.0f && t < 2.8f)
    {
      const float s = 0.5f + 0.4f * std::sin(9.f * t);
      v.push_back(
          rec(3, 0.5f, s, "confirmed", 0.7f, t - 1.0f, 0.f, 2.f * std::cos(9.f * t)));
    }
    h.send(v);
    h.tick();
  }
  // Everyone has left: allow grace, min-note-length and any rhythmic hold.
  h.send({});
  h.run_ms(4000.);
}

struct SceneStats
{
  int ons = 0, offs = 0, cc = 0, bend = 0, chan_pressure = 0, poly_at = 0;
  std::set<int> pitches, channels;
};

SceneStats analyse(const Harness& h)
{
  SceneStats s;
  for(const auto& m : h.log)
  {
    REQUIRE(!m.bytes.empty());
    const uint8_t st = m.bytes[0] & 0xF0;
    const int ch = (m.bytes[0] & 0x0F) + 1;
    for(std::size_t i = 1; i < m.bytes.size(); i++)
      REQUIRE(m.bytes[i] <= 127); // every data byte must be legal MIDI
    if(st == 0x90 && m.bytes.size() > 2 && m.bytes[2] > 0)
    {
      s.ons++;
      s.pitches.insert(m.bytes[1]);
      s.channels.insert(ch);
    }
    else if(st == 0x80 || (st == 0x90 && m.bytes.size() > 2 && m.bytes[2] == 0))
      s.offs++;
    else if(st == 0xB0)
      s.cc++;
    else if(st == 0xE0)
      s.bend++;
    else if(st == 0xD0)
      s.chan_pressure++;
    else if(st == 0xA0)
      s.poly_at++;
  }
  return s;
}
}

TEST_CASE(
    "every shipped preset produces usable MIDI on a simulated scene",
    "[entity_to_midi][presets]")
{
  namespace fs = std::filesystem;
  const fs::path dir{E2M_PRESET_DIR};
  if(!fs::exists(dir))
  {
    WARN("preset directory not found, skipping: " << dir.string());
    return;
  }

  int checked = 0;
  for(const auto& de : fs::directory_iterator(dir))
  {
    if(de.path().extension() != ".scp")
      continue;
    const auto lp = read_preset(de.path());
    REQUIRE(lp.has_value());
    INFO("preset: " << lp->name << "  (" << de.path().filename().string() << ")");
    REQUIRE(!lp->entries.empty());

    Harness h;
    for(const auto& e : lp->entries)
    {
      INFO("  entry index " << e.index << " value " << e.value);
      REQUIRE(apply_entry(h.p, e));
    }

    run_scene(h);
    const auto s = analyse(h);

    WARN(
        lp->name << ": ons=" << s.ons << " offs=" << s.offs
                 << " pitches=" << s.pitches.size() << " channels=" << s.channels.size()
                 << " cc=" << s.cc << " bend=" << s.bend
                 << " chanPress=" << s.chan_pressure << " polyAT=" << s.poly_at);

    // 1. It makes sound. Nothing emitted for three moving entities over four
    //    seconds is not a usable preset whatever its settings.
    REQUIRE(s.ons > 0);

    // 2. It finishes what it starts: nothing hanging once every entity has
    //    gone and all grace periods have elapsed.
    REQUIRE(h.chk.outstanding() == 0);
    REQUIRE(h.p.outstanding() == 0);
    REQUIRE(s.offs >= s.ons);

    // 3. Legal channels and pitches.
    for(int c : s.channels)
      REQUIRE((c >= 1 && c <= 16));
    for(int p : s.pitches)
      REQUIRE((p >= 0 && p <= 127));

    // 4. It conveys the movement it was given: several distinct pitches, or
    //    continuous control, or repeated triggering.
    const bool expressive = s.pitches.size() > 1 || s.cc > 0 || s.bend > 0
                            || s.chan_pressure > 0 || s.poly_at > 0 || s.ons > 3;
    REQUIRE(expressive);

    checked++;
  }
  REQUIRE(checked == 6);
}

// ---------------------------------------------------------------------------
// Input formats.
//
// The Entities inlet used to be typed std::vector<track_record>, which decodes
// a list POSITIONALLY by field order (id, slot, state, ...). Anything that was
// not already a full 12-field record therefore filled id and slot and never
// reached `position`, so every entity sat at (0,0,0) and the process emitted
// one single pitch no matter what was sent - identically for every preset.
//
// These cases pin the accepted family down by the only thing a user can
// observe: the pitches that reach the wire must depend on where the entities
// actually are.
// ---------------------------------------------------------------------------
namespace
{
//! Pitches emitted for one frame of entities, held long enough to sound.
std::vector<int> pitches_for(
    std::vector<ossia::value> v, E2MCoords mode = E2MCoords::TwoD)
{
  Harness h;
  h.p.inputs.coords.value = mode;
  h.send_raw(std::move(v));
  h.run_ms(200.);
  std::vector<int> out;
  for(const auto& m : h.log)
    if(m.bytes.size() > 2 && (m.bytes[0] & 0xF0) == 0x90 && m.bytes[2] > 0)
      out.push_back(m.bytes[1]);
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

ossia::value pos_map(float x, float y)
{
  ossia::value_map_type m;
  m.emplace_back("position", ossia::make_vec(x, y, 0.f));
  return m;
}
}

TEST_CASE("bare position formats reach the pitch mapping", "[entity_to_midi][formats]")
{
  using V = std::vector<ossia::value>;

  // Two entities at clearly different heights must give two different pitches.
  // Before the fix each of these produced exactly one pitch, always the same.
  const std::vector<std::pair<const char*, V>> cases{
      {"list of vec2f", V{ossia::make_vec(0.1f, 0.1f), ossia::make_vec(0.9f, 0.9f)}},
      {"list of [x,y] sub-lists", V{V{0.1f, 0.1f}, V{0.9f, 0.9f}}},
      {"list of {position} maps", V{pos_map(0.1f, 0.1f), pos_map(0.9f, 0.9f)}},
      {"flat [x,y,x,y]", V{0.1f, 0.1f, 0.9f, 0.9f}},
  };

  for(const auto& [label, v] : cases)
  {
    INFO(label);
    const auto p = pitches_for(v);
    REQUIRE(p.size() == 2);
    REQUIRE(p[0] != p[1]);
  }
}

TEST_CASE("pitch follows the position it is given", "[entity_to_midi][formats]")
{
  using V = std::vector<ossia::value>;
  // Pitch must move monotonically along the axis, and in the direction the
  // Invert control says. Y is the default axis and Invert Axis defaults ON
  // because camera coordinates grow downwards, so a LARGER y is lower on the
  // stage and must play a LOWER note.
  const auto sweep = [](bool invert) {
    std::vector<int> out;
    for(float y : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
    {
      Harness h;
      h.p.inputs.pitch_invert.value = invert;
      h.send_raw({ossia::make_vec(0.5f, y)});
      h.run_ms(200.);
      for(const auto& m : h.log)
        if(m.bytes.size() > 2 && (m.bytes[0] & 0xF0) == 0x90 && m.bytes[2] > 0)
        {
          out.push_back(m.bytes[1]);
          break;
        }
    }
    return out;
  };

  const auto inverted = sweep(true);
  REQUIRE(inverted.size() == 5);
  for(std::size_t i = 1; i < inverted.size(); i++)
    REQUIRE(inverted[i] <= inverted[i - 1]);
  REQUIRE(inverted.front() > inverted.back()); // and it spans a real range

  const auto upright = sweep(false);
  REQUIRE(upright.size() == 5);
  for(std::size_t i = 1; i < upright.size(); i++)
    REQUIRE(upright[i] >= upright[i - 1]);
  REQUIRE(upright.back() > upright.front());
}

TEST_CASE("a vec3 is read per the Coordinates mode", "[entity_to_midi][formats]")
{
  using V = std::vector<ossia::value>;
  // Same data, read two ways: as (x, y, z) the two entities differ in y only;
  // forced to 2D the third component becomes confidence and must not alter
  // pitch, which still comes from y.
  const V v{ossia::make_vec(0.5f, 0.2f, 0.9f), ossia::make_vec(0.5f, 0.8f, 0.3f)};

  const auto as3d = pitches_for(v, E2MCoords::ThreeD);
  const auto as2d = pitches_for(v, E2MCoords::TwoD);
  REQUIRE(as3d.size() == 2);
  REQUIRE(as2d.size() == 2);
  // Pitch is driven by Y in both readings, so the pitches agree; the point is
  // that neither collapses to a single value.
  REQUIRE(as2d[0] != as2d[1]);
  REQUIRE(as3d[0] != as3d[1]);
}

TEST_CASE("full records still take precedence over synthesis", "[entity_to_midi][formats]")
{
  // Ids carried by the source must be used rather than the list index, or a
  // reordered frame would silently retrigger every note.
  Harness h;
  ossia::value_map_type a, b;
  a.emplace_back("id", 41);
  a.emplace_back("position", ossia::make_vec(0.2f, 0.2f, 0.f));
  b.emplace_back("id", 42);
  b.emplace_back("position", ossia::make_vec(0.8f, 0.8f, 0.f));

  h.send_raw({a, b});
  h.run_ms(200.);
  const auto before = h.log.size();
  REQUIRE(before > 0);

  // Same two entities, opposite order: no new note-ons should appear.
  h.send_raw({b, a});
  h.run_ms(200.);
  int new_ons = 0;
  for(std::size_t i = before; i < h.log.size(); i++)
    if(h.log[i].bytes.size() > 2 && (h.log[i].bytes[0] & 0xF0) == 0x90
       && h.log[i].bytes[2] > 0)
      new_ons++;
  REQUIRE(new_ons == 0);
}

TEST_CASE("triggered + retrigger fires a new note per scale step", "[entity_to_midi]")
{
  // Documented behaviour: "Triggered: notes are short events fired when an
  // entity appears (and re-fired on pitch change in Retrigger tracking)".
  // One entity sweeping the whole pitch axis must therefore produce a series
  // of notes, not a single one.
  Harness h;
  h.p.inputs.note_model.value = avnd_tools::E2MNoteModel::Triggered;
  h.p.inputs.pitch_tracking.value = avnd_tools::E2MPitchTracking::Retrigger;
  h.p.inputs.scale.value = avnd_tools::E2MScale::Chromatic;
  // A realistic percussive lifetime. Strict isolation of the retrigger branch
  // is the next test's job; this one checks the end-to-end behaviour and that
  // nothing is left ringing.
  h.p.inputs.trigger_duration.value = 150.f;
  h.p.inputs.retrig_lockout.value = 20.f;
  h.p.inputs.min_note.value = 10.f;

  // Sweep bottom to top of the pitch range over 3 s. Invert Axis is on by
  // default, so y descending walks the pitch upwards; either way it crosses
  // many semitones.
  for(int i = 0; i < 180; i++)
  {
    const float t = i / 60.f;
    const float y = std::clamp(t / 3.f, 0.f, 1.f);
    h.send({rec(1, 0.5f, y, "confirmed", 0.9f, t, 0.f, 0.33f)});
    h.tick();
  }
  h.send({});
  h.run_ms(1000.);

  std::set<int> pitches;
  int ons = 0;
  for(const auto& m : h.log)
    if(m.bytes.size() > 2 && (m.bytes[0] & 0xF0) == 0x90 && m.bytes[2] > 0)
    {
      ons++;
      pitches.insert(m.bytes[1]);
    }

  INFO("note-ons: " << ons << "  distinct pitches: " << pitches.size());
  REQUIRE(ons > 1);
  REQUIRE(pitches.size() > 1);
  REQUIRE(h.chk.outstanding() == 0);
}

TEST_CASE("retrigger is what produces the extra notes", "[entity_to_midi]")
{
  // Control: the same sweep with Latched tracking must give exactly one note,
  // isolating the retrigger branch as the cause of every additional one.
  const auto sweep = [](avnd_tools::E2MPitchTracking mode) {
    Harness h;
    h.p.inputs.note_model.value = avnd_tools::E2MNoteModel::Triggered;
    h.p.inputs.pitch_tracking.value = mode;
    h.p.inputs.scale.value = avnd_tools::E2MScale::Chromatic;
    h.p.inputs.trigger_duration.value = 10000.f;
    h.p.inputs.retrig_lockout.value = 20.f;
    h.p.inputs.min_note.value = 10.f;
    for(int i = 0; i < 180; i++)
    {
      const float t = i / 60.f;
      h.send({rec(1, 0.5f, std::clamp(t / 3.f, 0.f, 1.f), "confirmed", 0.9f, t, 0.f, 0.33f)});
      h.tick();
    }
    int ons = 0;
    for(const auto& m : h.log)
      if(m.bytes.size() > 2 && (m.bytes[0] & 0xF0) == 0x90 && m.bytes[2] > 0)
        ons++;
    return ons;
  };

  const int latched = sweep(avnd_tools::E2MPitchTracking::Latched);
  const int retrig = sweep(avnd_tools::E2MPitchTracking::Retrigger);
  INFO("latched=" << latched << " retrigger=" << retrig);
  REQUIRE(latched == 1);
  REQUIRE(retrig > latched);
}

// ---------------------------------------------------------------------------
// Analysis descriptors.
//
// These feed Pressure From / Timbre From, and a descriptor that is merely
// plausible still produces perfectly reasonable-looking MIDI - so these assert
// on the analysis itself rather than on the notes it eventually drives.
// ---------------------------------------------------------------------------
namespace
{
using D = avnd_tools::entity_descriptors;

//! Drive a scene and return the descriptors of the last frame.
template <typename Fn>
ossia::flat_map<int, D> analyse_scene(Fn&& positions, int frames = 120, Harness* keep = nullptr)
{
  static Harness* h_ptr = nullptr;
  Harness local;
  Harness& h = keep ? *keep : local;
  h.p.inputs.desc_smooth.value = 0.f; // assert on the analysis, not the smoothing
  for(int i = 0; i < frames; i++)
  {
    h.send(positions(i / 60.f));
    h.tick();
  }
  (void)h_ptr;
  return h.p.descriptors();
}
}

TEST_CASE("signed motion descriptors are centred when still", "[entity_to_midi][analysis]")
{
  const auto d = analyse_scene([](float) {
    return std::vector<TR>{rec(1, 0.5f, 0.5f, "confirmed", 0.9f, 1.f, 0.f, 0.f)};
  });
  REQUIRE(d.size() == 1);
  const auto& e = d.begin()->second;
  // 0.5 is "no motion" for every bipolar source; anything else and a still
  // entity would sit off-centre on the receiving synth.
  REQUIRE(e.vx == Catch::Approx(0.5f).margin(1e-4));
  REQUIRE(e.vy == Catch::Approx(0.5f).margin(1e-4));
  REQUIRE(e.speed == Catch::Approx(0.f).margin(1e-4));
  REQUIRE(e.sx == Catch::Approx(0.f).margin(1e-4));
}

TEST_CASE("per-axis velocity carries direction", "[entity_to_midi][analysis]")
{
  // Moving right only: X is above centre, Y stays centred, |vy| stays zero.
  const auto right = analyse_scene([](float t) {
    return std::vector<TR>{rec(1, 0.2f + 0.2f * t, 0.5f, "confirmed", 0.9f, 1.f, 0.2f, 0.f)};
  });
  const auto& r = right.begin()->second;
  REQUIRE(r.vx > 0.5f);
  REQUIRE(r.vy == Catch::Approx(0.5f).margin(1e-4));
  REQUIRE(r.sx > 0.f);
  REQUIRE(r.sy == Catch::Approx(0.f).margin(1e-4));

  // Moving left: below centre, but the magnitude source is unsigned and so
  // reads the same as moving right at the same rate.
  const auto left = analyse_scene([](float t) {
    return std::vector<TR>{rec(1, 0.8f - 0.2f * t, 0.5f, "confirmed", 0.9f, 1.f, -0.2f, 0.f)};
  });
  const auto& l = left.begin()->second;
  REQUIRE(l.vx < 0.5f);
  REQUIRE(l.sx == Catch::Approx(r.sx).margin(1e-3));
}

TEST_CASE("acceleration responds to a change of velocity", "[entity_to_midi][analysis]")
{
  // Constant velocity: no acceleration.
  const auto steady = analyse_scene([](float t) {
    return std::vector<TR>{rec(1, 0.1f + 0.3f * t, 0.5f, "confirmed", 0.9f, 1.f, 0.3f, 0.f)};
  });
  REQUIRE(steady.begin()->second.accel == Catch::Approx(0.f).margin(1e-3));

  // Velocity ramping up: acceleration must be non-zero and above centre on
  // the signed X source.
  const auto ramp = analyse_scene([](float t) {
    const float v = 0.2f + 1.5f * t;
    return std::vector<TR>{rec(1, 0.1f + 0.2f * t * t, 0.5f, "confirmed", 0.9f, 1.f, v, 0.f)};
  });
  const auto& a = ramp.begin()->second;
  REQUIRE(a.accel > 0.f);
  REQUIRE(a.ax > 0.5f);
}

TEST_CASE("turn rate separates a curve from a straight line", "[entity_to_midi][analysis]")
{
  const auto straight = analyse_scene([](float t) {
    return std::vector<TR>{rec(1, 0.1f + 0.2f * t, 0.5f, "confirmed", 0.9f, 1.f, 0.2f, 0.f)};
  });
  // Circle: heading rotates at a constant rate.
  const auto circle = analyse_scene([](float t) {
    const float w = 2.f;
    return std::vector<TR>{rec(
        1, 0.5f + 0.2f * std::cos(w * t), 0.5f + 0.2f * std::sin(w * t), "confirmed",
        0.9f, 1.f, -0.2f * w * std::sin(w * t), 0.2f * w * std::cos(w * t))};
  });
  REQUIRE(straight.begin()->second.turn_rate == Catch::Approx(0.f).margin(1e-3));
  REQUIRE(circle.begin()->second.turn_rate > 0.05f);
  REQUIRE(circle.begin()->second.curvature > 0.f);
}

TEST_CASE("agitation separates erratic from purposeful at equal speed",
          "[entity_to_midi][analysis]")
{
  // Same mean speed, different character: a steady glide versus a jitter.
  const auto smoothly = analyse_scene([](float t) {
    return std::vector<TR>{rec(1, 0.1f + 0.2f * t, 0.5f, "confirmed", 0.9f, 1.f, 0.2f, 0.f)};
  });
  const auto jittery = analyse_scene([](float t) {
    const float v = 0.2f * ((int(t * 60.f) % 2) ? 1.f : -1.f);
    return std::vector<TR>{rec(1, 0.5f, 0.5f, "confirmed", 0.9f, 1.f, v, 0.f)};
  });
  REQUIRE(jittery.begin()->second.agitation > smoothly.begin()->second.agitation);
}

TEST_CASE("relational descriptors see the other entities", "[entity_to_midi][analysis]")
{
  Harness far_h, near_h;
  far_h.p.inputs.neighbour_range.value = 0.25f;
  near_h.p.inputs.neighbour_range.value = 0.25f;

  for(int i = 0; i < 60; i++)
  {
    far_h.send({rec(1, 0.05f, 0.5f), rec(2, 0.95f, 0.5f)});
    far_h.tick();
    near_h.send({rec(1, 0.48f, 0.5f), rec(2, 0.52f, 0.5f)});
    near_h.tick();
  }

  const auto& f = far_h.p.descriptors();
  const auto& n = near_h.p.descriptors();
  REQUIRE(f.size() == 2);
  REQUIRE(n.size() == 2);

  // Nearest-neighbour is normalised against Neighbour Range and saturates
  // when nobody is within it.
  REQUIRE(f.begin()->second.nearest == Catch::Approx(1.f).margin(1e-4));
  REQUIRE(n.begin()->second.nearest < 0.5f);

  // Density: nobody in range vs everybody in range.
  REQUIRE(f.begin()->second.density == Catch::Approx(0.f).margin(1e-4));
  REQUIRE(n.begin()->second.density == Catch::Approx(1.f).margin(1e-4));

  // Two entities are symmetric about their own centroid, and the spread-out
  // pair is further from it than the huddled one.
  REQUIRE(f.begin()->second.centroid_dist > n.begin()->second.centroid_dist);
}

TEST_CASE("radius and age", "[entity_to_midi][analysis]")
{
  Harness h;
  h.p.inputs.age_ref.value = 1.f;

  // Dead centre of the Position Min/Max window.
  for(int i = 0; i < 10; i++)
  {
    h.send({rec(1, 0.5f, 0.5f)});
    h.tick();
  }
  REQUIRE(h.p.descriptors().begin()->second.radius == Catch::Approx(0.f).margin(1e-3));

  // A corner is further out than the centre.
  Harness c;
  for(int i = 0; i < 10; i++)
  {
    c.send({rec(1, 1.f, 1.f)});
    c.tick();
  }
  REQUIRE(c.p.descriptors().begin()->second.radius > 0.5f);

  // Age climbs towards the reference and is clamped at it.
  Harness a;
  a.p.inputs.age_ref.value = 1.f;
  const float early = [&] {
    for(int i = 0; i < 12; i++)
    {
      a.send({rec(1, 0.5f, 0.5f, "confirmed", 0.9f, 0.1f)});
      a.tick();
    }
    return a.p.descriptors().begin()->second.age;
  }();
  for(int i = 0; i < 120; i++)
  {
    a.send({rec(1, 0.5f, 0.5f, "confirmed", 0.9f, 2.f)});
    a.tick();
  }
  const float late = a.p.descriptors().begin()->second.age;
  REQUIRE(late > early);
  REQUIRE(late == Catch::Approx(1.f).margin(1e-4)); // clamped at the reference
}

TEST_CASE("every descriptor stays inside 0..1 and finite", "[entity_to_midi][analysis]")
{
  // Adversarial: entities appearing, vanishing, teleporting and stacking on
  // top of each other. Nothing may escape the range a CC can carry.
  Harness h;
  std::mt19937 rng{1234};
  std::uniform_real_distribution<float> u{-2.f, 3.f};

  for(int i = 0; i < 400; i++)
  {
    std::vector<TR> v;
    const int n = i % 7;
    for(int e = 0; e < n; e++)
      v.push_back(rec(e + 1, u(rng), u(rng), "confirmed", 0.9f, 1.f, u(rng), u(rng)));
    h.send(v);
    h.tick();

    for(const auto& [id, d] : h.p.descriptors())
    {
      const float all[]
          = {d.x,        d.y,          d.z,          d.radius,     d.speed,
             d.vx,       d.vy,         d.vz,         d.sx,         d.sy,
             d.sz,       d.accel,      d.ax,         d.ay,         d.az,
             d.jerk,     d.turn_rate,  d.curvature,  d.heading_sin, d.heading_cos,
             d.agitation, d.nearest,   d.centroid_dist, d.density, d.age,
             d.confidence};
      for(float x : all)
      {
        INFO("entity " << id << " frame " << i);
        REQUIRE(std::isfinite(x));
        REQUIRE(x >= 0.f);
        REQUIRE(x <= 1.f);
      }
    }
  }
}
