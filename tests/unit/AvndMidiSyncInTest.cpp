#include <Midi/MidiSyncInput.hpp>

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
    // 01:02:03:04 at 25 fps
    for(uint8_t qf : {0x04, 0x10, 0x23, 0x30, 0x42, 0x50, 0x61, 0x72})
      d.send({0xF1, qf});
    d.tick(512);

    CHECK(d.fx.outputs.mtc_active.value);
    CHECK(d.fx.outputs.valid.value);
    CHECK(d.fx.outputs.frame_rate.value == Approx(25.));
    CHECK(d.fx.outputs.timecode.value == Approx(3600. + 120. + 3. + 4. / 25.));
    CHECK(!d.fx.outputs.clock_active.value);

    // Clocks interpolate between quarter frames.
    for(int i = 0; i < 12; i++)
      d.send({0xF8});
    d.tick(512);
    CHECK(d.fx.outputs.clock_active.value);
    CHECK(d.fx.outputs.timecode.value == Approx(3723.16 + 0.25));
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
