#include <Midi/MidiSyncInput.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_utils.hpp>
#include <ossia/network/context.hpp>
#include <ossia/protocols/midi/midi_device.hpp>
#include <ossia/protocols/midi/midi_protocol.hpp>

#include <avnd/binding/ossia/data_node.hpp>
#include <libremidi/configurations.hpp>
#include <libremidi/defaults.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <vector>

using Catch::Approx;
using Sync = mo::MIDISyncIn;

namespace
{
constexpr double rate = 48000.;

struct event
{
  int64_t at{};
  std::vector<uint8_t> bytes;
  bool operator==(const event&) const = default;
};

struct driver
{
  Sync fx;
  int64_t now{};
  std::vector<event> out;

  driver()
  {
    fx.prepare(halp::setup{
        .input_channels = 0, .output_channels = 0, .frames = 4096, .rate = rate});
  }

  void send(std::initializer_list<uint8_t> bytes, int ts = 0)
  {
    libremidi::message m;
    m.bytes.assign(bytes.begin(), bytes.end());
    m.timestamp = ts;
    fx.inputs.midi_in.push_back(m);
  }

  void tick(int frames)
  {
    fx.outputs.clock_out.midi_messages.clear();
    fx(frames);
    for(const auto& m : fx.outputs.clock_out.midi_messages)
    {
      if(frames > 0)
      {
        REQUIRE(m.timestamp >= 0);
        REQUIRE(m.timestamp < frames);
      }
      else
      {
        REQUIRE(m.timestamp == 0);
      }
      out.push_back(event{now + m.timestamp, {m.bytes.begin(), m.bytes.end()}});
    }
    fx.inputs.midi_in.midi_messages.clear();
    now += frames;
  }

  int count(uint8_t status) const
  {
    int n = 0;
    for(auto& e : out)
      n += (!e.bytes.empty() && e.bytes[0] == status);
    return n;
  }
};
}

TEST_CASE("MIDI Sync In decoding on a known clock stream", "[midi_sync_in]")
{
  SECTION("clock only")
  {
    driver d;
    d.fx.inputs.sync_mode.value = Sync::SyncMode::Clock_Only;
    // A filter of 1 holds Base Tempo: these clocks are not at 120 BPM
    d.fx.inputs.clock_filter.value = 1.f;
    d.send({0xFA});
    for(int i = 0; i < 48; i++)
      d.send({0xF8}, i);
    d.tick(512);

    CHECK(d.fx.outputs.clock_active.value);
    CHECK(!d.fx.outputs.mtc_active.value);
    CHECK(d.fx.outputs.valid.value);
    CHECK(d.fx.outputs.song_position.value == 0);
    CHECK(d.fx.outputs.estimated_tempo.value == Approx(120.));
    CHECK(d.fx.outputs.timecode.value == Approx(1.0));
    CHECK(d.fx.outputs.frame_rate.value == Approx(30.));

    d.send({0xF2, 0x10, 0x00});
    d.tick(512);
    CHECK(d.fx.outputs.song_position.value == 16);
    CHECK(d.fx.outputs.timecode.value == Approx(2.0));

    d.send({0xFC});
    d.tick(512);
    CHECK(!d.fx.outputs.valid.value);

    d.fx.inputs.format.value = Sync::OutputFormat::Milliseconds;
    d.send({0xFB});
    d.send({0xF8});
    d.tick(512);
    CHECK(d.fx.outputs.valid.value);
    CHECK(d.fx.outputs.timecode.value == Approx(97. / 24. * 0.5 * 1000.));
  }

  SECTION("MTC quarter frames")
  {
    driver d;
    // 01:02:03:04 at 25 fps: the last piece arrives two frames later
    for(uint8_t qf : {0x04, 0x10, 0x23, 0x30, 0x42, 0x50, 0x61, 0x72})
      d.send({0xF1, qf});
    d.tick(512);

    CHECK(d.fx.outputs.mtc_active.value);
    CHECK(d.fx.outputs.valid.value);
    CHECK(d.fx.outputs.frame_rate.value == Approx(25.));
    CHECK(d.fx.outputs.timecode.value == Approx(3600. + 120. + 3. + 6. / 25.));
    CHECK(!d.fx.outputs.clock_active.value);

    // Clocks interpolate between quarter frames.
    for(int i = 0; i < 12; i++)
      d.send({0xF8});
    d.tick(512);
    CHECK(d.fx.outputs.clock_active.value);
    CHECK(d.fx.outputs.timecode.value == Approx(3723.24 + 0.25));
  }

  SECTION("decoding does not depend on the clock out controls")
  {
    driver a, b;
    for(auto* d : {&a, &b})
      d->fx.inputs.sync_mode.value = Sync::SyncMode::Clock_Only;
    b.fx.inputs.clock_divide.value = 4;
    b.fx.inputs.start_shift.value = -3;
    b.fx.inputs.clock_delay.value = 20.f;
    b.fx.inputs.pass_spp.value = false;
    for(auto* d : {&a, &b})
    {
      d->send({0xFA});
      for(int i = 0; i < 30; i++)
        d->send({0xF8}, i * 10);
      d->send({0xF2, 0x05, 0x01}, 400);
      d->tick(512);
    }
    CHECK(a.fx.outputs.timecode.value == b.fx.outputs.timecode.value);
    CHECK(a.fx.outputs.song_position.value == b.fx.outputs.song_position.value);
    CHECK(a.fx.outputs.song_position.value == 133);
    CHECK(a.fx.outputs.valid.value == b.fx.outputs.valid.value);
  }
}

TEST_CASE("Clock out passes clock and transport through by default", "[midi_sync_in]")
{
  driver d;
  d.send({0xF2, 0x01, 0x02}, 0);
  d.send({0xFA}, 1);
  d.send({0x90, 60, 100}, 2);
  d.send({0xF8}, 3);
  d.send({0xF1, 0x04}, 4);
  d.send({0xF8}, 100);
  d.send({0xFC}, 200);
  d.send({0xFB}, 300);
  d.send({0xFE}, 301);
  d.tick(512);

  const std::vector<event> expected{
      {0, {0xF2, 0x01, 0x02}}, {1, {0xFA}},   {3, {0xF8}},
      {100, {0xF8}},           {200, {0xFC}}, {300, {0xFB}}};
  CHECK(d.out == expected);

  // Nothing is held back.
  d.tick(512);
  CHECK(d.out.size() == expected.size());
}

TEST_CASE("Clock out divides the clock from each Start", "[midi_sync_in]")
{
  driver d;
  d.fx.inputs.clock_divide.value = 3;
  d.send({0xFA});
  for(int i = 0; i < 10; i++)
    d.send({0xF8}, 10 + i);
  d.tick(256);
  CHECK(d.count(0xF8) == 4);
  CHECK(d.out[1].at == 10);
  CHECK(d.out[2].at == 13);
  CHECK(d.out[3].at == 16);
  CHECK(d.out[4].at == 19);

  // Continue keeps counting: clocks 10 and 11 were dropped, 12 passes.
  d.out.clear();
  d.send({0xFB});
  d.send({0xF8}, 1);
  d.send({0xF8}, 2);
  d.send({0xF8}, 3);
  d.tick(256);
  REQUIRE(d.out.size() == 2);
  CHECK(d.out[1].at == 256 + 3);

  // Start restarts the count.
  d.out.clear();
  d.send({0xFA});
  d.send({0xF8}, 5);
  d.tick(256);
  REQUIRE(d.out.size() == 2);
  CHECK(d.out[1] == event{512 + 5, {0xF8}});
}

TEST_CASE("Clock out shifts by N clocks at Start", "[midi_sync_in]")
{
  SECTION("positive: drops the first clocks")
  {
    driver d;
    d.fx.inputs.start_shift.value = 2;
    d.send({0xF8}, 0);
    d.send({0xFA}, 1);
    for(int i = 0; i < 5; i++)
      d.send({0xF8}, 10 + i);
    d.tick(256);
    REQUIRE(d.out.size() == 5);
    CHECK(d.out[0] == event{0, {0xF8}});
    CHECK(d.out[1] == event{1, {0xFA}});
    CHECK(d.out[2] == event{12, {0xF8}});

    // Continue does not shift.
    d.out.clear();
    d.send({0xFB});
    d.send({0xF8}, 4);
    d.tick(256);
    CHECK(d.count(0xF8) == 1);
  }

  SECTION("negative: inserts clocks after Start")
  {
    driver d;
    d.fx.inputs.start_shift.value = -3;
    d.send({0xFA}, 7);
    d.send({0xF8}, 20);
    d.tick(256);
    const std::vector<event> expected{
        {7, {0xFA}}, {7, {0xF8}}, {7, {0xF8}}, {7, {0xF8}}, {20, {0xF8}}};
    CHECK(d.out == expected);
  }

  SECTION("shift then divide")
  {
    driver d;
    d.fx.inputs.start_shift.value = 1;
    d.fx.inputs.clock_divide.value = 2;
    d.send({0xFA});
    for(int i = 0; i < 6; i++)
      d.send({0xF8}, 1 + i);
    d.tick(256);
    // Clock 1 dropped; 2, 4, 6 pass.
    REQUIRE(d.count(0xF8) == 3);
    CHECK(d.out[1].at == 2);
    CHECK(d.out[2].at == 4);
    CHECK(d.out[3].at == 6);
  }
}

TEST_CASE("Clock out delays every message", "[midi_sync_in]")
{
  driver d;
  d.fx.inputs.clock_delay.value = 10.f; // 480 frames
  d.send({0xFA}, 0);
  d.send({0xF8}, 100);
  d.send({0xF2, 0, 0}, 200);
  d.tick(256);
  CHECK(d.out.empty());
  d.tick(256);
  REQUIRE(d.out.size() == 1);
  CHECK(d.out[0] == event{480, {0xFA}});
  d.tick(256);
  REQUIRE(d.out.size() == 3);
  CHECK(d.out[1] == event{580, {0xF8}});
  CHECK(d.out[2] == event{680, {0xF2, 0, 0}});

  SECTION("a second of delay holds a second of clocks")
  {
    driver e;
    e.fx.inputs.clock_delay.value = 1000.f;
    // 300 BPM: 120 clocks per second, in 375 blocks of 128 frames.
    int64_t sent = 0;
    for(int b = 0; b < 375; b++)
    {
      if(b % 3 == 0)
      {
        e.send({0xF8}, 0);
        sent++;
      }
      e.tick(128);
    }
    for(int b = 0; b < 400; b++)
      e.tick(128);
    CHECK(e.count(0xF8) == sent);
    for(std::size_t i = 0; i < e.out.size(); i++)
      CHECK(e.out[i].at == int64_t(i) * 3 * 128 + 48000);
  }
}

TEST_CASE("Clock out drops SPP on request", "[midi_sync_in]")
{
  driver d;
  d.fx.inputs.pass_spp.value = false;
  d.send({0xF2, 0x10, 0x00});
  d.send({0xFA});
  d.tick(64);
  REQUIRE(d.out.size() == 1);
  CHECK(d.out[0].bytes[0] == 0xFA);
  // Decoding still sees it.
  d.fx.inputs.sync_mode.value = Sync::SyncMode::Clock_Only;
  d.send({0xF2, 0x10, 0x00});
  d.tick(64);
  CHECK(d.fx.outputs.song_position.value == 16);
}

TEST_CASE("Clock out on stop sends the delayed transport at once", "[midi_sync_in]")
{
  driver d;
  d.fx.inputs.clock_delay.value = 100.f;
  d.send({0xF8}, 0);
  d.send({0xFC}, 1);
  d.send({0xF8}, 2);
  d.tick(256);
  CHECK(d.out.empty());

  d.fx.stop();
  d.tick(0);
  REQUIRE(d.out.size() == 1);
  CHECK(d.out[0].bytes[0] == 0xFC);

  for(int i = 0; i < 40; i++)
    d.tick(256);
  CHECK(d.out.size() == 1);

  // The divider starts over after a stop.
  d.out.clear();
  d.fx.inputs.clock_delay.value = 0.f;
  d.fx.inputs.clock_divide.value = 2;
  d.send({0xF8}, 0);
  d.send({0xF8}, 1);
  d.tick(64);
  REQUIRE(d.out.size() == 1);
  CHECK(d.out[0].at == d.now - 64);
}

namespace
{
//! The 8 quarter frames of hh:mm:ss:ff, rate bits r, pieces [first, last].
void quarter_frames(driver& d, int h, int m, int s, int f, int r, int first = 0, int last = 7)
{
  const uint8_t data[8]{
      uint8_t(f & 0x0F),        uint8_t((f >> 4) & 0x01), uint8_t(s & 0x0F),
      uint8_t((s >> 4) & 0x03), uint8_t(m & 0x0F),        uint8_t((m >> 4) & 0x03),
      uint8_t(h & 0x0F),        uint8_t(((h >> 4) & 0x01) | (r << 1))};
  for(int i = first; i <= last; i++)
    d.send({0xF1, uint8_t((i << 4) | data[i])});
}

void full_frame(driver& d, int h, int m, int s, int f, int r)
{
  d.send({0xF0, 0x7F, 0x7F, 0x01, 0x01, uint8_t(h | (r << 5)), uint8_t(m), uint8_t(s),
          uint8_t(f), 0xF7});
}
}

TEST_CASE("MIDI Sync In: a full frame sets the position at once", "[midi_sync_in]")
{
  driver d;
  d.fx.inputs.sync_mode.value = Sync::SyncMode::MTC_Only;
  // 01:02:03:04 at 25 fps.
  full_frame(d, 1, 2, 3, 4, 0b01);
  d.tick(64);
  CHECK(d.fx.outputs.valid.value);
  CHECK(d.fx.outputs.frame_rate.value == Approx(25.));
  CHECK(d.fx.outputs.timecode.value == Approx(3723. + 4. / 25.));

  // Any device ID.
  d.send({0xF0, 0x7F, 0x05, 0x01, 0x01, uint8_t(2 | (0b11 << 5)), 0, 0, 15, 0xF7});
  d.tick(64);
  CHECK(d.fx.outputs.frame_rate.value == Approx(30.));
  CHECK(d.fx.outputs.timecode.value == Approx(7200. + 0.5));
}

TEST_CASE("MIDI Sync In: quarter frames from before a full frame are dropped", "[midi_sync_in]")
{
  driver d;
  d.fx.inputs.sync_mode.value = Sync::SyncMode::MTC_Only;
  quarter_frames(d, 0, 10, 20, 5, 0b11);
  d.tick(64);
  CHECK(d.fx.outputs.timecode.value == Approx(620. + 7. / 30.));

  // Half a sequence, then a locate.
  quarter_frames(d, 0, 10, 20, 7, 0b11, 0, 3);
  full_frame(d, 1, 0, 0, 0, 0b11);
  d.tick(64);
  CHECK(d.fx.outputs.timecode.value == Approx(3600.));

  // The other half does not complete the old sequence.
  quarter_frames(d, 0, 10, 20, 7, 0b11, 4, 7);
  d.tick(64);
  CHECK(d.fx.outputs.timecode.value == Approx(3600.));

  // Quarter frames still decode afterwards.
  quarter_frames(d, 1, 0, 2, 0, 0b11);
  d.tick(64);
  CHECK(d.fx.outputs.timecode.value == Approx(3602. + 2. / 30.));
}

TEST_CASE("MIDI Sync In: a full frame drops the clock interpolation", "[midi_sync_in]")
{
  driver d;
  d.fx.inputs.sync_mode.value = Sync::SyncMode::MTC_With_Clock_Interpolation;
  quarter_frames(d, 0, 0, 10, 0, 0b11);
  d.tick(64);
  CHECK(d.fx.outputs.timecode.value == Approx(10. + 2. / 30.));
  for(int i = 0; i < 24; i++)
    d.send({0xF8});
  d.tick(64);
  CHECK(d.fx.outputs.timecode.value > 10.1);

  full_frame(d, 0, 1, 0, 0, 0b11);
  d.tick(64);
  CHECK(d.fx.outputs.timecode.value == Approx(60.));
}

TEST_CASE("MIDI Sync In: quarter frames give the time the last one arrives", "[midi_sync_in]")
{
  // Piece 0 goes out at the start of the frame it carries, piece 7 a frame and
  // three quarters later: the next piece 0 carries two frames more
  driver d;
  d.fx.inputs.sync_mode.value = Sync::SyncMode::MTC_Only;
  quarter_frames(d, 0, 0, 1, 28, 0b11);
  d.tick(64);
  CHECK(d.fx.outputs.timecode.value == Approx(2.));
  quarter_frames(d, 0, 0, 2, 0, 0b11);
  d.tick(64);
  CHECK(d.fx.outputs.timecode.value == Approx(2. + 2. / 30.));
}

TEST_CASE("MIDI Sync In: the tempo comes from the clocks' timestamps", "[midi_sync_in]")
{
  // 125 BPM: a clock every 960 frames, in buffers of 256 that do not line up
  // with them, processed much faster than they would play
  driver d;
  d.fx.inputs.sync_mode.value = Sync::SyncMode::Clock_Only;
  d.fx.inputs.clock_filter.value = 0.f;
  d.send({0xFA});
  int64_t next = 0;
  for(int b = 0; b < 200; b++)
  {
    const int64_t start = d.now;
    while(next < start + 256)
    {
      d.send({0xF8}, int(next - start));
      next += 960;
    }
    d.tick(256);
  }
  CHECK(d.fx.outputs.estimated_tempo.value == Approx(125.));

  // The history keeps 24 intervals in place
  const auto& iv = d.fx.clock_interp.tick_intervals;
  CHECK(iv.size() == Sync::ClockInterpolation::max_interval_history);
#if defined(OSSIA_SMALL_VECTOR)
  CHECK(iv.capacity() == Sync::ClockInterpolation::max_interval_history);
#endif
}

TEST_CASE("Clock out follows Song Position through divide and shift", "[midi_sync_in]")
{
  SECTION("divide")
  {
    driver d;
    d.fx.inputs.clock_divide.value = 2;
    d.send({0xFA});
    for(int i = 0; i < 13; i++)
      d.send({0xF8}, 1 + i);
    d.send({0xFC}, 20);
    // Sixteenth 4 is clock 24: on the divided grid, and the 12th clock out,
    // so sixteenth 2 of the clock out
    d.send({0xF2, 4, 0}, 30);
    d.send({0xFB}, 31);
    d.send({0xF8}, 40);
    d.send({0xF8}, 41);
    d.send({0xF8}, 42);
    d.tick(256);
    std::vector<event> after;
    for(auto& e : d.out)
      if(e.at >= 30)
        after.push_back(e);
    const std::vector<event> expected{
        {30, {0xF2, 2, 0}}, {31, {0xFB}}, {40, {0xF8}}, {42, {0xF8}}};
    CHECK(after == expected);
  }

  SECTION("shift then divide")
  {
    driver d;
    d.fx.inputs.start_shift.value = 1;
    d.fx.inputs.clock_divide.value = 3;
    // Sixteenth 2 is clock 12: 11 clocks past the shift, the next to pass is
    // clock 13, the 5th clock out (sixteenth 0 of the clock out)
    d.send({0xF2, 2, 0}, 0);
    d.send({0xFB}, 1);
    for(int i = 0; i < 6; i++)
      d.send({0xF8}, 10 + i);
    d.tick(256);
    const std::vector<event> expected{
        {0, {0xF2, 0, 0}}, {1, {0xFB}}, {11, {0xF8}}, {14, {0xF8}}};
    CHECK(d.out == expected);
  }

  SECTION("a position before the shift drops the clocks up to it")
  {
    driver d;
    d.fx.inputs.start_shift.value = 8;
    d.send({0xF2, 1, 0}, 0); // clock 6
    d.send({0xFB}, 1);
    for(int i = 0; i < 4; i++)
      d.send({0xF8}, 10 + i);
    d.tick(256);
    const std::vector<event> expected{{0, {0xF2, 0, 0}}, {1, {0xFB}}, {12, {0xF8}}, {13, {0xF8}}};
    CHECK(d.out == expected);
  }
}

// From the wire to the object: a full frame read by a MIDI input device, carried
// by the dataflow graph as SysEx7 packets, and handed to MIDI Sync In through
// the avendish binding.

namespace
{
struct sync_node_rig
{
  ossia::execution_state state;
  oscr::safe_node<Sync> node{64, rate, 1};
  ossia::inlet* midi_in{};

  sync_node_rig()
  {
    state.sampleRate = int(rate);
    state.bufferSize = 64;
    node.finish_init();
    node.audio_configuration_changed({&state});
    node.prepare(state);
    node.impl.effect.inputs.sync_mode.value = Sync::SyncMode::MTC_Only;
    for(auto* in : node.root_inputs())
      if(in->target<ossia::midi_port>())
        midi_in = in;
    REQUIRE(midi_in);
  }

  auto& packets() { return midi_in->target<ossia::midi_port>()->messages; }

  void run()
  {
    ossia::token_request token;
    token.prev_date = ossia::time_value{0};
    token.date = ossia::time_value{64};
    token.start_sample = 0;
    token.length_sample = 64;
    node.requested_tokens.clear();
    node.request(token);
    ossia::graph_util::exec_node(node, state);
    packets().clear();
  }
};

std::vector<libremidi::ump> packets_of(std::initializer_list<uint8_t> bytes)
{
  const std::vector<uint8_t> v{bytes};
  libremidi::midi1_to_midi2 conv;
  std::vector<libremidi::ump> packets;
  conv.convert_packets(
      v.data(), v.size(), 0, [&](const libremidi::ump& p) { packets.push_back(p); });
  return packets;
}
}

TEST_CASE("MIDI Sync In node: a full frame from a MIDI input device locates the timecode", "[midi_sync_in]")
{
  sync_node_rig rig;

  auto ctx = std::make_shared<ossia::net::network_context>();
  libremidi::input_configuration conf;
  ossia::net::midi::midi_device dev{
      "midi", std::make_unique<ossia::net::midi::midi_protocol>(
                  ctx, ossia::net::midi::midi_protocol_configuration{"midi"}, conf,
                  libremidi::midi_in_configuration_for(libremidi::API::KEYBOARD))};
  REQUIRE(!conf.ignore_sysex);

  rig.state.register_device(&dev);
  rig.midi_in->address = &dev.get_root_node();
  rig.state.register_port(*rig.midi_in);
  rig.state.begin_tick();

  // 01:02:03:04 at 25 fps
  libremidi::message frame;
  frame.bytes = {0xF0, 0x7F, 0x7F, 0x01, 0x01, uint8_t(1 | (0b01 << 5)), 2, 3, 4, 0xF7};
  conf.on_message(std::move(frame));
  rig.state.begin_tick();
  rig.state.copy_from_global_node(dev.get_root_node(), *rig.midi_in);
  REQUIRE(rig.packets().size() == 2);
  rig.run();

  auto& out = rig.node.impl.effect.outputs;
  CHECK(out.valid.value);
  CHECK(out.mtc_active.value);
  CHECK(out.frame_rate.value == Approx(25.));
  CHECK(out.timecode.value == Approx(3723. + 4. / 25.));

  rig.state.unregister_port(*rig.midi_in);
  rig.state.unregister_device(&dev);
  rig.state.begin_tick();
}

TEST_CASE("MIDI Sync In node: a full frame split over two buffers locates once complete", "[midi_sync_in]")
{
  sync_node_rig rig;
  // 02:00:00:15 at 30 fps, from another process's MIDI outlet
  const auto packets
      = packets_of({0xF0, 0x7F, 0x7F, 0x01, 0x01, uint8_t(2 | (0b11 << 5)), 0, 0, 15, 0xF7});
  REQUIRE(packets.size() == 2);

  rig.packets().push_back(packets[0]);
  rig.run();
  CHECK(!rig.node.impl.effect.outputs.valid.value);

  rig.packets().push_back(packets[1]);
  rig.run();
  auto& out = rig.node.impl.effect.outputs;
  CHECK(out.valid.value);
  CHECK(out.frame_rate.value == Approx(30.));
  CHECK(out.timecode.value == Approx(7200. + 0.5));
}
