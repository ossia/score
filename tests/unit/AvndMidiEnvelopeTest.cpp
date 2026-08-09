#include <Advanced/MidiScaler/MidiEnvelope.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using Catch::Approx;
using Env = mtk::MidiEnvelope;

namespace
{
// A sample rate of 1000 makes one sample exactly one millisecond, so the
// stage times below can be read directly as sample counts.
constexpr double rate = 1000.;

struct driver
{
  Env fx;

  driver()
  {
    fx.prepare(halp::setup{
        .input_channels = 0, .output_channels = 0, .frames = 64, .rate = rate});
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

  //! Run one processing block. The host clears the ports around each tick, so
  //! we emulate that here.
  void tick(int frames)
  {
    fx.outputs.out.values.clear();
    fx.outputs.midi.midi_messages.clear();
    fx(halp::tick_musical{.frames = frames});
    fx.inputs.midi.midi_messages.clear();
  }

  //! Value of the envelope at the end of the last block
  float last() const
  {
    REQUIRE(!fx.outputs.out.values.empty());
    return fx.outputs.out.values.rbegin()->second;
  }
};
}

TEST_CASE("mtk::MidiEnvelope ADSR follows its stage times", "[avnd][midi][envelope]")
{
  driver d;
  auto& in = d.fx.inputs;
  in.attack.value = 0.010f;  // 10 samples
  in.decay.value = 0.100f;   // 100 samples
  in.sustain.value = 0.5f;
  in.release.value = 0.050f; // 50 samples

  SECTION("attack rises linearly from 0 to the peak")
  {
    // A long, linear decay so that what happens just past the attack does not
    // pollute the measurement of the peak.
    in.decay.value = 10.f;
    in.decay_curve.value = 0.f;

    d.note_on(60, 127);
    d.tick(10);
    CHECK(d.last() == Approx(1.f).margin(1e-3));

    // Halfway through a fresh attack
    driver e;
    e.fx.inputs.attack.value = 0.010f;
    e.note_on(60, 127);
    e.tick(5);
    CHECK(e.last() == Approx(0.5f).margin(1e-5));
  }

  SECTION("decay settles on the sustain level and stays there")
  {
    d.note_on(60, 127);
    d.tick(10);  // end of attack
    d.tick(100); // end of decay
    CHECK(d.last() == Approx(0.5f).margin(1e-3));

    // Sustain does not elapse: the level is unchanged after long blocks
    d.tick(64);
    CHECK(d.last() == Approx(0.5f).margin(1e-3));
    d.tick(64);
    CHECK(d.last() == Approx(0.5f).margin(1e-3));
  }

  SECTION("note off releases down to zero")
  {
    d.note_on(60, 127);
    d.tick(10);
    d.tick(100);
    CHECK(d.fx.outputs.gate.value == 1.f);

    d.note_off(60);
    d.tick(60); // comfortably past the 50-sample release
    CHECK(d.last() == Approx(0.f).margin(1e-3));
    CHECK(d.fx.outputs.gate.value == 0.f);
    CHECK(d.fx.outputs.active.value == 0);
  }
}

TEST_CASE(
    "mtk::MidiEnvelope honours the note-on sample offset", "[avnd][midi][envelope]")
{
  driver d;
  d.fx.inputs.attack.value = 0.010f; // 10 samples
  d.fx.inputs.decay.value = 10.f;    // long enough not to interfere
  d.fx.inputs.decay_curve.value = 0.f;

  // The note starts 20 samples into a 32-sample block: 12 samples of attack
  // elapse, so the (10-sample) attack has completed by the end of the block.
  d.note_on(60, 127, 20);
  d.tick(32);
  CHECK(d.last() == Approx(1.f).margin(1e-3));

  // Same block, but the note starts late enough that the attack is unfinished.
  driver e;
  e.fx.inputs.attack.value = 0.032f; // 32 samples
  e.fx.inputs.decay.value = 10.f;
  e.note_on(60, 127, 24);
  e.tick(32); // only 8 of the 32 attack samples elapsed
  CHECK(e.last() == Approx(8.f / 32.f).margin(1e-2));
}

TEST_CASE("mtk::MidiEnvelope scales the peak by velocity", "[avnd][midi][envelope]")
{
  SECTION("velocity amount 1: peak is the normalized velocity")
  {
    driver d;
    d.fx.inputs.attack.value = 0.010f;
    d.fx.inputs.decay.value = 10.f;
    d.fx.inputs.velocity_amount.value = 1.f;
    d.note_on(60, 64);
    d.tick(10);
    CHECK(d.last() == Approx(64.f / 127.f).margin(1e-3));
  }

  SECTION("velocity amount 0: velocity is ignored, peak is always 1")
  {
    driver d;
    d.fx.inputs.attack.value = 0.010f;
    d.fx.inputs.decay.value = 10.f;
    d.fx.inputs.velocity_amount.value = 0.f;
    d.note_on(60, 1);
    d.tick(10);
    CHECK(d.last() == Approx(1.f).margin(1e-3));
  }
}

TEST_CASE("mtk::MidiEnvelope filters by channel and key range", "[avnd][midi][envelope]")
{
  SECTION("notes outside the key range are ignored")
  {
    driver d;
    d.fx.inputs.attack.value = 0.010f;
    d.fx.inputs.key_range.value = {60.f, 72.f};

    d.note_on(48, 127); // below
    d.tick(10);
    CHECK(d.last() == Approx(0.f).margin(1e-6));
    CHECK(d.fx.outputs.active.value == 0);

    d.note_on(64, 127); // inside
    d.tick(10);
    CHECK(d.last() > 0.5f);
  }

  SECTION("channel 0 accepts everything, a set channel filters")
  {
    driver d;
    d.fx.inputs.attack.value = 0.010f;
    d.fx.inputs.channel.value = 3;

    d.note_on(60, 127, 0, /* channel */ 1);
    d.tick(10);
    CHECK(d.last() == Approx(0.f).margin(1e-6));

    d.note_on(60, 127, 0, /* channel */ 3);
    d.tick(10);
    CHECK(d.last() > 0.5f);
  }
}

TEST_CASE(
    "mtk::MidiEnvelope polyphony allocates one voice per note",
    "[avnd][midi][envelope]")
{
  driver d;
  auto& in = d.fx.inputs;
  in.voicing.value = Env::Poly;
  in.voice_count.value = 4;
  in.attack.value = 0.010f;
  in.decay.value = 10.f;
  in.velocity_amount.value = 1.f;

  d.note_on(60, 127);
  d.note_on(64, 64);
  d.note_on(67, 32);
  d.tick(10);

  CHECK(d.fx.outputs.active.value == 3);
  REQUIRE(d.fx.outputs.voices.value.size() == 4);

  // Max combine: the loudest voice wins
  CHECK(d.last() == Approx(1.f).margin(1e-3));

  SECTION("each voice carries its own velocity-scaled level")
  {
    const auto& v = d.fx.outputs.voices.value;
    CHECK(v[0] == Approx(1.f).margin(1e-3));
    CHECK(v[1] == Approx(64.f / 127.f).margin(1e-3));
    CHECK(v[2] == Approx(32.f / 127.f).margin(1e-3));
    CHECK(v[3] == Approx(0.f).margin(1e-6));
  }

  SECTION("releasing one note leaves the others running")
  {
    d.fx.inputs.release.value = 0.001f;
    d.note_off(64);
    d.tick(4);
    CHECK(d.fx.outputs.active.value == 2);
  }

  SECTION("more notes than voices steals the oldest")
  {
    d.note_on(70, 127);
    d.note_on(72, 127); // 5th note on a 4-voice engine
    d.tick(10);
    CHECK(d.fx.outputs.active.value == 4);
  }
}

TEST_CASE("mtk::MidiEnvelope mono mode uses the note priority", "[avnd][midi][envelope]")
{
  driver d;
  auto& in = d.fx.inputs;
  in.attack.value = 0.f;
  in.decay.value = 10.f;
  in.velocity_amount.value = 0.f;

  SECTION("last-note priority falls back to the previous note on release")
  {
    in.priority.value = Env::Last;
    d.note_on(60, 100);
    d.tick(4);
    REQUIRE(d.fx.outputs.pitch.value.has_value());
    CHECK(*d.fx.outputs.pitch.value == Approx(60.f));

    d.note_on(67, 100);
    d.tick(4);
    CHECK(*d.fx.outputs.pitch.value == Approx(67.f));

    // Releasing the top note falls back to the one still held
    d.note_off(67);
    d.tick(4);
    CHECK(*d.fx.outputs.pitch.value == Approx(60.f));
    CHECK(d.fx.outputs.gate.value == 1.f);
  }

  SECTION("lowest-note priority")
  {
    in.priority.value = Env::Lowest;
    d.note_on(72, 100);
    d.note_on(48, 100);
    d.note_on(60, 100);
    d.tick(4);
    REQUIRE(d.fx.outputs.pitch.value.has_value());
    CHECK(*d.fx.outputs.pitch.value == Approx(48.f));
  }

  SECTION("highest-note priority")
  {
    in.priority.value = Env::Highest;
    d.note_on(72, 100);
    d.note_on(48, 100);
    d.note_on(60, 100);
    d.tick(4);
    REQUIRE(d.fx.outputs.pitch.value.has_value());
    CHECK(*d.fx.outputs.pitch.value == Approx(72.f));
  }
}

TEST_CASE("mtk::MidiEnvelope AD mode is percussive", "[avnd][midi][envelope]")
{
  driver d;
  auto& in = d.fx.inputs;
  in.envelope.value = Env::AD;
  in.attack.value = 0.010f;
  in.decay.value = 0.020f;
  in.decay_curve.value = 0.f;

  d.note_on(60, 127);
  d.tick(10);
  CHECK(d.last() == Approx(1.f).margin(1e-3));

  // The note-off is ignored: the decay plays to completion
  d.note_off(60);
  d.tick(10);
  CHECK(d.last() > 0.f);
  CHECK(d.fx.outputs.active.value == 1);

  d.tick(20);
  CHECK(d.last() == Approx(0.f).margin(1e-5));
  CHECK(d.fx.outputs.active.value == 0);
}

TEST_CASE(
    "mtk::MidiEnvelope maps the output into the output range",
    "[avnd][midi][envelope]")
{
  SECTION("a plain range offsets and scales")
  {
    driver d;
    d.fx.inputs.attack.value = 0.010f;
    d.fx.inputs.decay.value = 10.f;
    d.fx.inputs.output_range.value = {20.f, 120.f};

    d.tick(4); // idle
    CHECK(d.last() == Approx(20.f).margin(1e-3));

    d.note_on(60, 127);
    d.tick(10);
    CHECK(d.last() == Approx(120.f).margin(1e-2));
  }

  SECTION("an inverted range flips the envelope")
  {
    driver d;
    d.fx.inputs.attack.value = 0.010f;
    d.fx.inputs.decay.value = 10.f;
    d.fx.inputs.output_range.value = {1.f, 0.f};

    d.tick(4);
    CHECK(d.last() == Approx(1.f).margin(1e-3));

    d.note_on(60, 127);
    d.tick(10);
    CHECK(d.last() == Approx(0.f).margin(1e-2));
  }
}

TEST_CASE("mtk::MidiEnvelope curve modes use the drawn curve", "[avnd][midi][envelope]")
{
  SECTION("an empty curve behaves as the identity ramp")
  {
    driver d;
    d.fx.inputs.envelope.value = Env::Curve;
    d.fx.inputs.curve_duration.value = 0.100f; // 100 samples

    d.note_on(60, 127);
    d.tick(50);
    CHECK(d.last() == Approx(0.5f).margin(1e-2));

    d.tick(60); // comfortably past the 100-sample curve
    CHECK(d.fx.outputs.active.value == 0); // one-shot: finished
  }

  SECTION("the curve acts as a transfer function in the ADSR modes")
  {
    driver d;
    // A curve mapping everything to 0.25
    d.fx.inputs.curve.value.push_back(halp::custom_curve_segment{
        .start = 0.f, .end = 1.f, .function = [](float) { return 0.25f; }});
    d.fx.inputs.attack.value = 0.010f;
    d.fx.inputs.decay.value = 10.f;

    d.note_on(60, 127);
    d.tick(10);
    CHECK(d.last() == Approx(0.25f).margin(1e-3));
  }
}

TEST_CASE("mtk::MidiEnvelope passes MIDI through", "[avnd][midi][envelope]")
{
  driver d;
  d.note_on(60, 127, 0);
  d.note_on(64, 100, 8);
  d.tick(16);

  REQUIRE(d.fx.outputs.midi.midi_messages.size() == 2);
  CHECK(d.fx.outputs.midi.midi_messages[0].bytes[1] == 60);
  CHECK(d.fx.outputs.midi.midi_messages[1].bytes[1] == 64);
  CHECK(d.fx.outputs.midi.midi_messages[1].timestamp == 8);
}

TEST_CASE("mtk::MidiEnvelope survives degenerate settings", "[avnd][midi][envelope]")
{
  SECTION("all stage times zero: the envelope collapses without hanging")
  {
    driver d;
    auto& in = d.fx.inputs;
    in.envelope.value = Env::DAHDSR;
    in.delay.value = 0.f;
    in.attack.value = 0.f;
    in.hold.value = 0.f;
    in.decay.value = 0.f;
    in.release.value = 0.f;
    in.sustain.value = 0.3f;

    d.note_on(60, 127);
    d.tick(16);
    CHECK(d.last() == Approx(0.3f).margin(1e-4));

    d.note_off(60);
    d.tick(16);
    CHECK(d.last() == Approx(0.f).margin(1e-4));
  }

  SECTION("a zero-length block does nothing")
  {
    driver d;
    d.note_on(60, 127);
    d.fx.outputs.out.values.clear();
    d.fx(halp::tick_musical{.frames = 0});
    CHECK(d.fx.outputs.out.values.empty());
  }

  SECTION("all-notes-off releases everything")
  {
    driver d;
    d.fx.inputs.voicing.value = Env::Poly;
    d.fx.inputs.voice_count.value = 4;
    d.fx.inputs.release.value = 0.001f;
    d.note_on(60, 127);
    d.note_on(64, 127);
    d.tick(4);
    CHECK(d.fx.outputs.gate.value == 1.f);

    auto cc = libremidi::channel_events::control_change(1, 123, 0);
    cc.timestamp = 0;
    d.fx.inputs.midi.push_back(cc);
    d.tick(8);
    CHECK(d.fx.outputs.gate.value == 0.f);
    CHECK(d.fx.outputs.active.value == 0);
  }
}
