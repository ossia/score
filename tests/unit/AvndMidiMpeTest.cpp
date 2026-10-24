#include <Advanced/MidiScaler/MidiMpe.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <vector>

using Mpe = mtk::MidiMpe;
using Mode = Mpe::mode_control;

namespace
{
struct out_msg
{
  int64_t at{};
  libremidi::message msg;
};

struct driver
{
  Mpe fx;
  int64_t now{};
  std::vector<out_msg> out;
  std::map<std::pair<int, int>, int> sounding;

  driver()
  {
    fx.prepare(halp::setup{.input_channels = 0, .output_channels = 0, .frames = 512, .rate = 48000.});
  }

  void send(libremidi::message m, int ts = 0)
  {
    m.timestamp = ts;
    fx.inputs.midi.push_back(m);
  }
  void note_on(int note, int ch = 1, int ts = 0, int vel = 100)
  {
    send(libremidi::channel_events::note_on(ch, note, vel), ts);
  }
  void note_off(int note, int ch = 1, int ts = 0)
  {
    send(libremidi::channel_events::note_off(ch, note, 0), ts);
  }

  void tick(int frames = 512)
  {
    fx.outputs.midi.midi_messages.clear();
    fx(halp::tick{frames});
    int64_t last = 0;
    for(auto& m : fx.outputs.midi.midi_messages)
    {
      if(frames > 0)
      {
        REQUIRE(m.timestamp >= 0);
        REQUIRE(m.timestamp < frames);
      }
      else
        REQUIRE(m.timestamp == 0);
      REQUIRE(m.timestamp >= last);
      last = m.timestamp;
      if(mtk::is_note_on(m))
      {
        auto& s = sounding[{mtk::channel_of(m), m.bytes[1]}];
        REQUIRE(s == 0);
        s++;
      }
      else if(mtk::is_note_off(m))
      {
        auto& s = sounding[{mtk::channel_of(m), m.bytes[1]}];
        REQUIRE(s == 1);
        s--;
      }
      out.push_back({now + m.timestamp, m});
    }
    fx.inputs.midi.midi_messages.clear();
    now += frames;
  }

  int total_sounding() const
  {
    int n = 0;
    for(auto& [k, v] : sounding)
      n += v;
    return n;
  }

  std::vector<libremidi::message> of_kind(mtk::midi_kind k) const
  {
    std::vector<libremidi::message> r;
    for(auto& e : out)
      if(mtk::classify(e.msg) == k)
        r.push_back(e.msg);
    return r;
  }
};

int bend_of(const libremidi::message& m)
{
  return m.bytes[1] | (m.bytes[2] << 7);
}

bool is(const libremidi::message& m, std::initializer_list<int> bytes)
{
  if(m.size() != bytes.size())
    return false;
  int i = 0;
  for(int b : bytes)
    if(m.bytes[i++] != b)
      return false;
  return true;
}
}

TEST_CASE("Midi MPE: MIDI to MPE spreads notes over members", "[midi_mpe]")
{
  driver d;
  d.note_on(60);
  d.note_on(64, 1, 7);
  d.tick();
  REQUIRE(d.out.size() == 8);
  // Each note starts from a clean expression on its own member channel.
  CHECK(is(d.out[0].msg, {0xE1, 0x00, 0x40}));
  CHECK(is(d.out[1].msg, {0xB1, 74, 64}));
  CHECK(is(d.out[2].msg, {0xD1, 0}));
  CHECK(is(d.out[3].msg, {0x91, 60, 100}));
  CHECK(is(d.out[4].msg, {0xE2, 0x00, 0x40}));
  CHECK(is(d.out[7].msg, {0x92, 64, 100}));
  CHECK(d.out[7].at == 7);

  d.out.clear();
  d.note_off(60);
  d.send(libremidi::channel_events::note_on(1, 64, 0));
  d.tick();
  REQUIRE(d.out.size() == 2);
  CHECK(is(d.out[0].msg, {0x81, 60, 0}));
  CHECK(is(d.out[1].msg, {0x82, 64, 0}));
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi MPE: MIDI to MPE routes expression", "[midi_mpe]")
{
  for(auto route : {Mpe::route_control::newest, Mpe::route_control::all})
  {
    driver d;
    d.fx.inputs.route.value = route;
    d.fx.inputs.midi_bend_range.value = 12;
    d.note_on(60);
    d.note_on(64);
    d.tick();
    d.out.clear();

    // +6 semitones on a 12 semitone keyboard is +6 on a 48 semitone member.
    d.send(libremidi::channel_events::pitch_bend(1, 8192 + 4096));
    d.send(libremidi::channel_events::aftertouch(1, 80));
    d.send(libremidi::channel_events::control_change(1, 74, 20));
    d.tick();
    std::vector<int> chs;
    for(auto& m : d.of_kind(mtk::midi_kind::pitch_bend))
    {
      CHECK(bend_of(m) == 8192 + 1024);
      chs.push_back(mtk::channel_of(m));
    }
    if(route == Mpe::route_control::newest)
      CHECK(chs == std::vector<int>{3});
    else
      CHECK(chs == std::vector<int>{2, 3});
    CHECK(d.of_kind(mtk::midi_kind::channel_pressure).size() == chs.size());
    CHECK(d.of_kind(mtk::midi_kind::control_change).size() == chs.size());

    // A new note starts with the current keyboard expression.
    d.out.clear();
    d.note_on(67);
    d.tick();
    REQUIRE(d.out.size() == 4);
    CHECK(is(d.out[0].msg, {0xE3, 0x00, (8192 + 1024) >> 7}));
    CHECK(is(d.out[1].msg, {0xB3, 74, 20}));
    CHECK(is(d.out[2].msg, {0xD3, 80}));

    // Poly pressure becomes the channel pressure of the note's member.
    d.out.clear();
    d.send(libremidi::channel_events::poly_pressure(1, 60, 33));
    d.tick();
    REQUIRE(d.out.size() == 1);
    CHECK(is(d.out[0].msg, {0xD1, 33}));

    // Other controllers and program changes go to the master channel.
    d.out.clear();
    d.send(libremidi::channel_events::control_change(5, 1, 99));
    d.send(libremidi::channel_events::program_change(5, 3));
    d.send(libremidi::message{0xF8});
    d.tick();
    REQUIRE(d.out.size() == 3);
    CHECK(is(d.out[0].msg, {0xB0, 1, 99}));
    CHECK(is(d.out[1].msg, {0xC0, 3}));
    CHECK(is(d.out[2].msg, {0xF8}));

    // CC 123 ends every note and reaches the whole zone.
    d.out.clear();
    d.send(libremidi::channel_events::control_change(1, 123, 0));
    d.tick();
    CHECK(d.total_sounding() == 0);
    CHECK(d.of_kind(mtk::midi_kind::control_change).size() == 16);
  }
}

TEST_CASE("Midi MPE: MIDI to MPE with a small zone shares channels", "[midi_mpe]")
{
  driver d;
  d.fx.inputs.members.value = 3;
  d.fx.inputs.input_channel.value = 1;
  for(int p : {60, 62, 64, 65})
    d.note_on(p);
  d.note_on(70, 6); // not the input channel: passes unchanged
  d.tick();
  std::vector<int> chs;
  for(auto& m : d.of_kind(mtk::midi_kind::note_on))
    chs.push_back(mtk::channel_of(m));
  CHECK(chs == std::vector<int>{2, 3, 4, 2, 6});

  for(int p : {60, 62, 64, 65})
    d.note_off(p);
  d.note_off(70, 6);
  d.tick();
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi MPE: MPE configuration message", "[midi_mpe]")
{
  driver d;
  d.fx.inputs.send_config.value = true;
  d.fx.inputs.members.value = 2;
  d.fx.inputs.mpe_bend_range.value = 24;
  d.tick();
  REQUIRE(d.out.size() == 18);
  const int rpn6[6][3] = {{0xB0, 101, 0}, {0xB0, 100, 6}, {0xB0, 6, 2},
                          {0xB0, 38, 0},  {0xB0, 101, 127}, {0xB0, 100, 127}};
  for(int i = 0; i < 6; i++)
    CHECK(is(d.out[i].msg, {rpn6[i][0], rpn6[i][1], rpn6[i][2]}));
  for(int ch = 2; ch <= 3; ch++)
  {
    const int base = 6 * (ch - 1);
    const int st = 0xB0 + ch - 1;
    CHECK(is(d.out[base + 0].msg, {st, 101, 0}));
    CHECK(is(d.out[base + 1].msg, {st, 100, 0}));
    CHECK(is(d.out[base + 2].msg, {st, 6, 24}));
  }

  // Sent once, again when the zone changes.
  d.out.clear();
  d.tick();
  CHECK(d.out.empty());
  d.fx.inputs.members.value = 5;
  d.tick();
  CHECK(d.out.size() == 36);
}

TEST_CASE("Midi MPE: MPE to MIDI collapses the zone", "[midi_mpe]")
{
  driver d;
  d.fx.inputs.mode.value = Mode::mpe_to_midi;
  d.fx.inputs.midi_bend_range.value = 12;

  d.send(libremidi::channel_events::pitch_bend(2, 8192 + 1024)); // +6 semitones
  d.note_on(60, 2);
  d.tick();
  REQUIRE(d.out.size() == 4);
  CHECK(is(d.out[0].msg, {0xE0, 0x00, 12288 >> 7}));
  CHECK(is(d.out[1].msg, {0xB0, 74, 64}));
  CHECK(is(d.out[2].msg, {0xD0, 0}));
  CHECK(is(d.out[3].msg, {0x90, 60, 100}));

  // The newest note's bend wins.
  d.out.clear();
  d.send(libremidi::channel_events::pitch_bend(3, 8192 - 512)); // -3 semitones
  d.note_on(64, 3);
  d.tick();
  REQUIRE(d.out.size() == 2);
  CHECK(bend_of(d.out[0].msg) == 8192 - 2048);
  CHECK(is(d.out[1].msg, {0x90, 64, 100}));

  // Highest pressure of the held notes.
  d.out.clear();
  d.send(libremidi::channel_events::aftertouch(2, 100));
  d.send(libremidi::channel_events::aftertouch(3, 50));
  d.tick();
  REQUIRE(d.out.size() == 1);
  CHECK(is(d.out[0].msg, {0xD0, 100}));

  // When the newest note ends, the previous one's bend comes back.
  d.out.clear();
  d.note_off(64, 3);
  d.send(libremidi::channel_events::control_change(2, 74, 90));
  d.send(libremidi::channel_events::control_change(2, 7, 90));
  d.tick();
  REQUIRE(d.out.size() == 4);
  CHECK(is(d.out[0].msg, {0x80, 64, 0}));
  CHECK(bend_of(d.out[1].msg) == 12288);
  CHECK(is(d.out[2].msg, {0xB0, 74, 90}));
  CHECK(is(d.out[3].msg, {0xB0, 7, 90}));

  // The same pitch on two members is one note on the output.
  d.note_on(60, 4);
  d.note_off(60, 2);
  d.tick();
  CHECK(d.total_sounding() == 1);
  d.note_off(60, 4);
  d.tick();
  CHECK(d.total_sounding() == 0);

  // Channels outside the zone pass unchanged.
  d.fx.inputs.members.value = 3;
  d.tick();
  d.out.clear();
  d.note_on(50, 9);
  d.tick();
  REQUIRE(d.out.size() == 1);
  CHECK(is(d.out[0].msg, {0x98, 50, 100}));
  d.note_off(50, 9);
  d.tick();
}

TEST_CASE("Midi MPE: MPE to MIDI reads the member RPNs", "[midi_mpe]")
{
  driver d;
  d.fx.inputs.mode.value = Mode::mpe_to_midi;
  d.fx.inputs.midi_bend_range.value = 12;
  d.tick();
  d.out.clear();

  // The pitch bend range of each member, as an MPE controller announces it.
  auto cc = [&](int ch, int n, int v) {
    d.send(libremidi::channel_events::control_change(ch, n, v));
  };
  for(int ch = 2; ch <= 16; ch++)
  {
    cc(ch, 101, 0);
    cc(ch, 100, 0);
    cc(ch, 6, 24);
    cc(ch, 38, 0);
    cc(ch, 101, 127);
    cc(ch, 100, 127);
  }
  // An NRPN with increments: also for the member, not for the output.
  cc(5, 99, 1);
  cc(5, 98, 2);
  cc(5, 96, 1);
  cc(5, 97, 1);
  cc(5, 6, 3);
  // Other controllers of members and of the master still pass.
  cc(3, 7, 90);
  cc(1, 101, 0);
  d.tick();
  REQUIRE(d.out.size() == 2);
  CHECK(is(d.out[0].msg, {0xB0, 7, 90}));
  CHECK(is(d.out[1].msg, {0xB0, 101, 0}));

  // +6 semitones out of the announced 24, on an output range of 12.
  d.out.clear();
  d.send(libremidi::channel_events::pitch_bend(2, 8192 + 2048));
  d.note_on(60, 2);
  d.tick();
  REQUIRE(!d.out.empty());
  CHECK(mtk::classify(d.out[0].msg) == mtk::midi_kind::pitch_bend);
  CHECK(bend_of(d.out[0].msg) == 8192 + 4096);
  d.note_off(60, 2);
  d.tick();
  CHECK(d.total_sounding() == 0);

  // The control takes over again when it changes.
  d.fx.inputs.mpe_bend_range.value = 12;
  d.out.clear();
  d.send(libremidi::channel_events::pitch_bend(2, 8192 + 2048));
  d.note_on(62, 2);
  d.tick();
  REQUIRE(!d.out.empty());
  CHECK(bend_of(d.out[0].msg) == 8192 + 2048);
  d.note_off(62, 2);
  d.tick();
}

TEST_CASE("Midi MPE: MPE to MPE shaping", "[midi_mpe]")
{
  driver d;
  d.fx.inputs.mode.value = Mode::mpe_to_mpe;

  // Neutral settings: everything passes unchanged.
  const std::vector<libremidi::message> in{
      libremidi::channel_events::pitch_bend(2, 9000),
      libremidi::channel_events::control_change(2, 74, 30),
      libremidi::channel_events::aftertouch(2, 17),
      libremidi::channel_events::note_on(2, 60, 100),
      libremidi::channel_events::control_change(1, 64, 127),
      libremidi::channel_events::pitch_bend(1, 3000),
      libremidi::channel_events::note_off(2, 60, 40)};
  for(auto& m : in)
    d.send(m);
  d.tick();
  REQUIRE(d.out.size() == in.size());
  for(std::size_t i = 0; i < in.size(); i++)
    CHECK(d.out[i].msg.bytes == in[i].bytes);

  d.out.clear();
  d.fx.inputs.pressure_curve.value = 2.f;
  d.fx.inputs.slide_curve.value = 0.5f;
  d.fx.inputs.bend_scale.value = 0.5f;
  d.send(libremidi::channel_events::aftertouch(3, 64));
  d.send(libremidi::channel_events::control_change(3, 74, 32));
  d.send(libremidi::channel_events::pitch_bend(3, 8192 + 1024));
  d.send(libremidi::channel_events::pitch_bend(1, 8192 + 1024)); // master: untouched
  d.tick();
  REQUIRE(d.out.size() == 4);
  CHECK(is(d.out[0].msg, {0xD2, 32}));
  CHECK(is(d.out[1].msg, {0xB2, 74, 64}));
  CHECK(bend_of(d.out[2].msg) == 8192 + 512);
  CHECK(bend_of(d.out[3].msg) == 8192 + 1024);
}

TEST_CASE("Midi MPE: seeded randomization at note start", "[midi_mpe]")
{
  auto run = [](int seed, Mode::enum_type mode) {
    driver d;
    d.fx.inputs.mode.value = mode;
    d.fx.inputs.random_bend.value = 1.f;
    d.fx.inputs.random_pressure.value = 0.5f;
    d.fx.inputs.random_slide.value = 0.25f;
    d.fx.inputs.seed.value = seed;
    for(int i = 0; i < 10; i++)
    {
      d.note_on(60 + i, mode == Mode::midi_to_mpe ? 1 : 2 + i);
      d.tick();
    }
    for(int i = 0; i < 10; i++)
      d.note_off(60 + i, mode == Mode::midi_to_mpe ? 1 : 2 + i);
    d.tick();
    CHECK(d.total_sounding() == 0);
    return d.out;
  };

  for(auto mode : {Mode::midi_to_mpe, Mode::mpe_to_mpe})
  {
    const auto a = run(11, mode);
    const auto b = run(11, mode);
    const auto c = run(12, mode);
    REQUIRE(a.size() == b.size());
    for(std::size_t i = 0; i < a.size(); i++)
      CHECK(a[i].msg.bytes == b[i].msg.bytes);
    bool differs = a.size() != c.size();
    for(std::size_t i = 0; !differs && i < a.size(); i++)
      differs = a[i].msg.bytes != c[i].msg.bytes;
    CHECK(differs);

    int distinct_bends = 0;
    int last = -1;
    for(auto& e : a)
    {
      switch(mtk::classify(e.msg))
      {
        case mtk::midi_kind::pitch_bend: {
          const double semis = (bend_of(e.msg) - 8192) / 8192. * 48.;
          CHECK(std::abs(semis) <= 1.01);
          if(bend_of(e.msg) != last)
            distinct_bends++;
          last = bend_of(e.msg);
          break;
        }
        case mtk::midi_kind::channel_pressure:
          CHECK(e.msg.bytes[1] <= 64);
          break;
        case mtk::midi_kind::control_change:
          CHECK(std::abs(int(e.msg.bytes[2]) - 64) <= 32);
          break;
        default:
          break;
      }
    }
    CHECK(distinct_bends >= 8);
  }
}

TEST_CASE("Midi MPE: mode change and stop end held notes", "[midi_mpe]")
{
  driver d;
  d.note_on(60);
  d.note_on(62);
  d.tick();
  CHECK(d.total_sounding() == 2);
  d.fx.inputs.mode.value = Mode::mpe_to_midi;
  d.tick();
  CHECK(d.total_sounding() == 0);

  d.note_on(60, 2);
  d.note_on(62, 3);
  d.tick();
  CHECK(d.total_sounding() == 2);
  d.fx.stop();
  d.out.clear();
  d.tick(0);
  CHECK(d.total_sounding() == 0);
  CHECK(d.of_kind(mtk::midi_kind::note_off).size() == 2);

  d.fx.inputs.mode.value = Mode::mpe_to_mpe;
  d.note_on(60, 2);
  d.tick();
  d.fx.stop();
  d.tick(0);
  CHECK(d.total_sounding() == 0);
}

TEST_CASE("Midi MPE: note-offs passed through keep the member busy", "[midi_mpe]")
{
  driver d;
  d.fx.inputs.members.value = 3;
  d.fx.inputs.input_channel.value = 1;
  d.fx.inputs.route.value = Mpe::route_control::all;
  d.note_on(60);
  d.tick();
  // A note on another input channel, on the member's channel: it passes, and
  // its note-off does not free member 2, where 60 sounds.
  d.note_on(70, 2);
  d.tick();
  d.note_off(70, 2);
  d.tick();
  d.out.clear();
  d.send(libremidi::channel_events::pitch_bend(1, 0x2000 + 0x800));
  d.tick();
  bool bend_on_2 = false;
  for(auto& m : d.of_kind(mtk::midi_kind::pitch_bend))
    bend_on_2 = bend_on_2 || mtk::channel_of(m) == 2;
  CHECK(bend_on_2);
  d.note_off(60);
  d.tick();
  CHECK(d.total_sounding() == 0);
}
