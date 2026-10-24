// The MIDI Sync Out presets of the default package load onto the object and
// produce, on the audio path, the clock, transport and timecode they are named
// for; the transport, the timecode and the free clock's queue behave across
// seeks, stops and loops. What the clock thread sends to a MIDI device is
// checked against the wall clock in the [.realtime] cases: they depend on the
// load of the machine and do not run by default.

#include "MidiPresetHarness.hpp"

#include <AvndProcesses/MIDISync.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <ossia/network/generic/generic_device.hpp>

#include <array>
#include <chrono>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

using Catch::Approx;

namespace
{
constexpr double rate = 48000.;
constexpr int frames = 512;
constexpr int64_t flicks_per_second = 705'600'000;

struct event
{
  int64_t at{};
  std::vector<uint8_t> bytes;
};

struct driver
{
  mtk::MIDISyncOut fx;
  int64_t now{};
  std::vector<event> out;

  driver() { fx.prepare(halp::setup{.frames = frames, .rate = rate}); }

  //! Plays `seconds` at 120 BPM from the start of the timeline.
  void play(double seconds, double tempo = 120.) { play_from(0, seconds, tempo); }

  //! Plays `seconds` from the timeline frame `first`.
  void play_from(int64_t first, double seconds, double tempo = 120.)
  {
    played.push_back({now, first, tempo});
    const int64_t n = int64_t(seconds * rate / frames);
    for(int64_t i = 0; i < n; i++)
    {
      halp::tick_flicks tk{};
      tk.frames = frames;
      tk.tempo = tempo;
      tk.signature = {4, 4};
      const int64_t f0 = first + i * frames;
      const int64_t f1 = f0 + frames;
      tk.start_in_flicks = int64_t(f0 / rate * flicks_per_second);
      tk.end_in_flicks = int64_t(f1 / rate * flicks_per_second);
      tk.start_position_in_quarters = f0 / rate * tempo / 60.;
      tk.end_position_in_quarters = f1 / rate * tempo / 60.;
      tk.relative_position = double(f0) / 1e6;
      tk.position_in_frames = f0;

      fx.outputs.midi.midi_messages.clear();
      fx(tk);
      for(const auto& m : fx.outputs.midi.midi_messages)
      {
        REQUIRE(m.timestamp >= 0);
        REQUIRE(m.timestamp < frames);
        out.push_back(event{now + m.timestamp, {m.bytes.begin(), m.bytes.end()}});
      }
      now += frames;
    }
  }

  //! The tick the executor gives after stop().
  void stop()
  {
    fx.stop();
    halp::tick_flicks tk{};
    tk.frames = 0;
    tk.tempo = 120.;
    fx.outputs.midi.midi_messages.clear();
    fx(tk);
    for(const auto& m : fx.outputs.midi.midi_messages)
      out.push_back(event{now, {m.bytes.begin(), m.bytes.end()}});
  }

  //! What went out from frame `from` on.
  std::vector<event> since(int64_t from) const
  {
    std::vector<event> r;
    for(auto& e : out)
      if(e.at >= from)
        r.push_back(e);
    return r;
  }

  double seconds() const { return now / rate; }

  //! A stretch of timeline played: from the output frame `at`, the timeline
  //! frame `first` on, at `tempo`.
  struct segment
  {
    int64_t at{};
    int64_t first{};
    double tempo{};
  };
  std::vector<segment> played;

  //! The position of the timeline, in quarters, at the output frame `at`.
  double quarters_at(int64_t at) const
  {
    const segment* s = &played.front();
    for(auto& p : played)
      if(p.at <= at)
        s = &p;
    return double(s->first + (at - s->at)) / rate * s->tempo / 60.;
  }

  int count(uint8_t status) const
  {
    int n = 0;
    for(auto& e : out)
      n += (!e.bytes.empty() && e.bytes[0] == status);
    return n;
  }

  std::vector<int64_t> dates(uint8_t status) const
  {
    std::vector<int64_t> d;
    for(auto& e : out)
      if(!e.bytes.empty() && e.bytes[0] == status)
        d.push_back(e.at);
    return d;
  }

  //! Data byte of every quarter frame of the given piece (0 to 7).
  std::vector<uint8_t> quarter_frames(int piece) const
  {
    std::vector<uint8_t> d;
    for(auto& e : out)
      if(e.bytes.size() == 2 && e.bytes[0] == 0xF1 && (e.bytes[1] >> 4) == piece)
        d.push_back(e.bytes[1]);
    return d;
  }
};

std::map<QString, midi_presets::preset> load_all()
{
  std::map<QString, midi_presets::preset> res;
  for(auto& p :
      midi_presets::load("MIDI Sync Out", "aa7c1ae5-495e-436e-a079-e3f1a19861bb"))
    res[p.name] = p;
  return res;
}

void apply(driver& d, const midi_presets::preset& p)
{
  const auto errors = midi_presets::apply(d.fx.inputs, p);
  INFO(errors.join("; ").toStdString());
  REQUIRE(errors.isEmpty());
}

const midi_presets::preset& get(const std::map<QString, midi_presets::preset>& all, const char* name)
{
  auto it = all.find(QString::fromUtf8(name));
  INFO(name);
  REQUIRE(it != all.end());
  return it->second;
}
}

TEST_CASE("MIDI Sync Out presets load", "[presets][midi_sync_out]")
{
  const auto all = load_all();
  REQUIRE(all.size() >= 10);
  for(auto& [name, p] : all)
  {
    INFO(name.toStdString());
    driver d;
    apply(d, p);
    d.play(0.5);
  }
}

TEST_CASE("MIDI Sync Out preset: clock only following score", "[presets][midi_sync_out]")
{
  const auto all = load_all();
  driver d;
  apply(d, get(all, "Clock only following score"));
  d.play(2.0);
  // 120 BPM, 24 per quarter: 48 clocks a second.
  CHECK(d.count(0xF8) == Approx(d.seconds() * 48.).margin(1));
  CHECK(d.count(0xFA) == 0);
  CHECK(d.count(0xFC) == 0);
  CHECK(d.count(0xF1) == 0);
  // The song position goes out once, at the start.
  CHECK(d.count(0xF2) == 1);
  const auto t = d.dates(0xF8);
  REQUIRE(t.size() > 2);
  CHECK(t[1] - t[0] == Approx(rate / 48.).margin(1));
}

TEST_CASE("MIDI Sync Out preset: clock and transport", "[presets][midi_sync_out]")
{
  const auto all = load_all();
  driver d;
  apply(d, get(all, "Clock and transport"));
  d.play(1.0);
  CHECK(d.count(0xFA) == 1);
  CHECK(d.count(0xF8) == Approx(d.seconds() * 48.).margin(1));
  REQUIRE(!d.out.empty());
  CHECK(d.out.front().bytes[0] == 0xFA);
  CHECK(d.count(0xF1) == 0);
}

TEST_CASE("MIDI Sync Out preset: half and double time clock", "[presets][midi_sync_out]")
{
  const auto all = load_all();
  driver half, dbl;
  apply(half, get(all, "Half time clock"));
  apply(dbl, get(all, "Double time clock"));
  half.play(2.0);
  dbl.play(2.0);
  CHECK(half.count(0xF8) == Approx(half.seconds() * 24.).margin(1));
  CHECK(dbl.count(0xF8) == Approx(dbl.seconds() * 96.).margin(1));
  CHECK(half.count(0xFA) == 1);
  CHECK(dbl.count(0xFA) == 1);
}

TEST_CASE("MIDI Sync Out preset: clock 20 ms early", "[presets][midi_sync_out]")
{
  const auto all = load_all();
  driver early, ref;
  apply(early, get(all, "Clock 20 ms early"));
  apply(ref, get(all, "Clock and transport"));
  early.play(2.0);
  ref.play(2.0);
  const auto a = early.dates(0xF8);
  const auto b = ref.dates(0xF8);
  REQUIRE(a.size() > 50);
  REQUIRE(b.size() > 50);
  // Past the start, every clock goes out 20 ms (960 frames) earlier.
  CHECK(b[40] - a[40] == Approx(0.020 * rate).margin(2));
  CHECK(a.size() == b.size() + 1);
}

namespace
{
//! Quarter frames per second, and the rate code of piece 7.
void check_mtc(const driver& d, double fps, int code)
{
  CHECK(d.count(0xF1) == Approx(d.seconds() * fps * 4.).margin(1));
  const auto p7 = d.quarter_frames(7);
  REQUIRE(!p7.empty());
  for(auto b : p7)
    CHECK(((b >> 1) & 0b11) == code);
  CHECK(d.count(0xF8) == 0);
  CHECK(d.count(0xFA) == 0);
}
}

TEST_CASE("MIDI Sync Out preset: MTC 25 fps", "[presets][midi_sync_out]")
{
  const auto all = load_all();
  driver d;
  apply(d, get(all, "MTC 25 fps"));
  d.play(4.0);
  check_mtc(d, 25., 0b01);
  // Hours stay at 0 without an offset.
  for(auto b : d.quarter_frames(6))
    CHECK((b & 0x0F) == 0);
}

TEST_CASE("MIDI Sync Out preset: MTC 30 fps starting at one hour", "[presets][midi_sync_out]")
{
  const auto all = load_all();
  driver d;
  apply(d, get(all, "MTC 30 fps starting at one hour"));
  d.play(30.0);
  // MTC rate code 3 is 30 fps.
  check_mtc(d, 30., 0b11);
  const auto h = d.quarter_frames(6);
  REQUIRE(!h.empty());
  for(auto b : h)
    CHECK((b & 0x0F) == 1);
}

TEST_CASE("MIDI Sync Out preset: MTC 29.97 fps for video", "[presets][midi_sync_out]")
{
  const auto all = load_all();
  driver d;
  apply(d, get(all, "MTC 29.97 fps for video"));
  d.play(30.0);
  // MTC rate code 2 is 29.97 fps (drop frame).
  check_mtc(d, 29.97, 0b10);
}

TEST_CASE("MIDI Sync Out preset: free clock 120 BPM with transport", "[presets][midi_sync_out]")
{
  const auto all = load_all();
  driver d;
  apply(d, get(all, "Free clock 120 BPM with transport"));
  d.play(1.0);
  // The score clock and transport are silent; the free clock gets its settings.
  CHECK(d.count(0xF8) == 0);
  CHECK(d.count(0xFA) == 0);
  CHECK(d.fx.free_mode.load());
  CHECK(d.fx.free_bpm.load() == Approx(120.f));
  CHECK(d.fx.free_send_clock.load());
  CHECK(d.fx.free_send_transport.load());
  CHECK(d.fx.clock_rate_factor.load() == Approx(1.f));
  CHECK(d.fx.outputs.tempo.value == Approx(120.f));
}

TEST_CASE("MIDI Sync Out preset: everything enabled", "[presets][midi_sync_out]")
{
  const auto all = load_all();
  driver d;
  apply(d, get(all, "Everything enabled"));
  d.play(2.0);
  CHECK(d.count(0xFA) == 1);
  CHECK(d.count(0xF8) == Approx(d.seconds() * 48.).margin(1));
  CHECK(d.count(0xF1) == Approx(d.seconds() * 25. * 4.).margin(1));
  CHECK(d.fx.free_mode.load() == false);
}

namespace
{
void enable_all(driver& d)
{
  d.fx.inputs.clock.value = mtk::MidiClockMode::Enabled;
  d.fx.inputs.clock_startstop.value = mtk::MidiStartStopMode::Enabled;
  d.fx.inputs.mtc.value = mtk::MidiTimeCodeMode::Enabled;
  d.fx.inputs.rate.value = mtk::MidiTimeCodeFrameRate::SMPTE_25;
}

std::vector<uint8_t> statuses(const std::vector<event>& evs, int64_t at)
{
  std::vector<uint8_t> r;
  for(auto& e : evs)
    if(e.at == at && !e.bytes.empty())
      r.push_back(e.bytes[0]);
  return r;
}
}

TEST_CASE("MIDI Sync Out: a loop starting over is a seek to its start", "[midi_sync_out]")
{
  driver d;
  enable_all(d);
  d.play(1.0);
  const int64_t loop = d.now;
  const int first_pass_clocks = d.count(0xF8);
  REQUIRE(first_pass_clocks > 40);

  // The timeline starts over at 0.
  d.play(1.0);
  const auto after = d.since(loop);

  // Stop, Start, then a full timecode frame at 00:00:00:00.
  const auto at_loop = statuses(after, loop);
  REQUIRE(at_loop.size() >= 3);
  CHECK(at_loop[0] == 0xFC);
  CHECK(at_loop[1] == 0xFA);
  CHECK(at_loop[2] == 0xF0);
  for(auto& e : after)
    if(e.at == loop && e.bytes[0] == 0xF0)
      CHECK(e.bytes == std::vector<uint8_t>{0xF0, 0x7F, 0x7F, 0x01, 0x01, 0x20, 0, 0, 0, 0xF7});

  // The clock and the timecode go on as on the first pass.
  int clocks = 0, quarter_frames = 0;
  int64_t first_clock = -1;
  int first_piece = -1;
  for(auto& e : after)
  {
    if(e.bytes[0] == 0xF8)
    {
      if(first_clock < 0)
        first_clock = e.at;
      clocks++;
    }
    if(e.bytes[0] == 0xF1)
    {
      if(first_piece < 0)
        first_piece = e.bytes[1] >> 4;
      quarter_frames++;
    }
  }
  CHECK(clocks == first_pass_clocks);
  CHECK(first_clock - loop == d.dates(0xF8).front());
  CHECK(quarter_frames == Approx(d.seconds() / 2. * 100.).margin(2));
  CHECK(first_piece == 0);
  CHECK(d.count(0xFA) == 2);
}

TEST_CASE("MIDI Sync Out: a jump into the middle sends Song Position and Continue", "[midi_sync_out]")
{
  driver d;
  d.fx.inputs.clock.value = mtk::MidiClockMode::Enabled;
  d.fx.inputs.clock_startstop.value = mtk::MidiStartStopMode::Enabled;
  d.play(1.0);
  const int64_t jump = d.now;

  // 49152 frames at 120 BPM: 2.048 quarters, past the 16th note 8. The
  // receiver resumes on the next one, 9, at 2.25 quarters.
  d.play_from(49152, 0.5);
  const auto after = d.since(jump);
  const auto at_jump = statuses(after, jump);
  REQUIRE(at_jump.size() >= 3);
  CHECK(at_jump[0] == 0xFC);
  CHECK(at_jump[1] == 0xF2);
  CHECK(at_jump[2] == 0xFB);
  for(auto& e : after)
    if(e.bytes[0] == 0xF2)
      CHECK(e.bytes == std::vector<uint8_t>{0xF2, 9, 0});

  // No clock before that 16th note: 2.25 quarters are 1.125 s, frame 54000.
  int64_t first_clock = -1;
  for(auto& e : after)
    if(e.bytes[0] == 0xF8)
    {
      first_clock = e.at;
      break;
    }
  CHECK(first_clock - jump == Approx(54000 - 49152).margin(1));
}

namespace
{
//! A clock follower as the MIDI specification describes it: Start sets its
//! position to 0, Song Position to that many 16th notes, and each clock
//! received while running plays the position, then moves it on by a clock.
struct receiver
{
  struct beat
  {
    int64_t at{};
    //! The receiver's position at that clock, in its quarters.
    double quarters{};
  };

  std::vector<beat> clocks;
  int64_t position{};
  bool running{};

  void feed(const std::vector<event>& evs)
  {
    for(auto& e : evs)
    {
      switch(e.bytes.empty() ? 0 : e.bytes[0])
      {
        case 0xF2:
          position = 6 * (e.bytes[1] | (e.bytes[2] << 7));
          break;
        case 0xFA:
          position = 0;
          running = true;
          break;
        case 0xFB:
          running = true;
          break;
        case 0xFC:
          running = false;
          break;
        case 0xF8:
          if(running)
            clocks.push_back({e.at, position++ / 24.});
          break;
      }
    }
  }
};

//! Every clock a receiver gets puts it where the timeline is: its quarters
//! are `factor` times the timeline's, within a frame.
void check_receiver_follows(const driver& d, double factor)
{
  receiver r;
  r.feed(d.out);
  REQUIRE(r.clocks.size() > 10);
  const double frames_per_quarter = rate * 60. / 120.;
  for(auto& c : r.clocks)
  {
    const double timeline = d.quarters_at(c.at);
    INFO("clock at frame " << c.at << ", timeline " << timeline << " quarters");
    CHECK(
        (c.quarters / factor - timeline) * frames_per_quarter
        == Approx(0.).margin(1.01));
  }
}
}

TEST_CASE("MIDI Sync Out: a receiver is where the timeline is after any jump", "[midi_sync_out]")
{
  // Positions in frames at 120 BPM, 24000 to a quarter: on a 16th note, on a
  // clock between two 16ths, between two clocks.
  const int64_t targets[] = {48000, 49000, 49152, 6000 + 4 * 1000, 123457, 6001};
  const int rates[] = {0, 1, 2, 3, 4, 5, 6};
  for(int rate_index : rates)
  {
    const double factor = mtk::midi_clock::clock_rate_factor(rate_index);
    for(int64_t target : targets)
    {
      INFO("clock rate x" << factor << ", jump to frame " << target);
      driver d;
      d.fx.inputs.clock.value = mtk::MidiClockMode::Enabled;
      d.fx.inputs.clock_startstop.value = mtk::MidiStartStopMode::Enabled;
      d.fx.inputs.clock_rate.value = mtk::MidiClockRate(rate_index);
      d.play(0.5);
      d.play_from(target, 1.0);
      // And a loop back to the start
      d.play_from(0, 0.5);
      check_receiver_follows(d, factor);
    }
  }
}

TEST_CASE("MIDI Sync Out: a receiver is where the timeline is after a stop and a start in the middle", "[midi_sync_out]")
{
  driver d;
  d.fx.inputs.clock.value = mtk::MidiClockMode::Enabled;
  d.fx.inputs.clock_startstop.value = mtk::MidiStartStopMode::Enabled;
  d.play(0.5);
  d.stop();
  d.fx.start();
  d.play_from(31111, 1.0);
  check_receiver_follows(d, 1.);
}

TEST_CASE("MIDI Sync Out: the first clock goes out with Start", "[midi_sync_out]")
{
  driver d;
  d.fx.inputs.clock.value = mtk::MidiClockMode::Enabled;
  d.fx.inputs.clock_startstop.value = mtk::MidiStartStopMode::Enabled;
  d.play(1.0);
  CHECK(statuses(d.out, 0) == std::vector<uint8_t>{0xFA, 0xF8});
  const auto t = d.dates(0xF8);
  REQUIRE(t.size() > 2);
  CHECK(t[1] == Approx(rate / 48.).margin(1));

  // A loop back to the start: Stop, Start and its clock
  const int64_t loop = d.now;
  d.play(0.5);
  CHECK(statuses(d.out, loop) == std::vector<uint8_t>{0xFC, 0xFA, 0xF8});
}

TEST_CASE("MIDI Sync Out: Stop, then a new Start from the beginning", "[midi_sync_out]")
{
  driver d;
  d.fx.inputs.clock.value = mtk::MidiClockMode::Enabled;
  d.fx.inputs.clock_startstop.value = mtk::MidiStartStopMode::Enabled;
  d.play(1.0);
  d.stop();
  CHECK(d.count(0xFC) == 1);
  // The extra tick of the executor does not send it twice.
  d.stop();
  CHECK(d.count(0xFC) == 1);

  const int64_t restart = d.now;
  const int before = d.count(0xF8);
  d.fx.start();
  d.play(1.0);
  CHECK(d.count(0xFA) == 2);
  // The Stop of the extra tick, then the Start and its clock.
  CHECK(statuses(d.since(restart), restart) == std::vector<uint8_t>{0xFC, 0xFA, 0xF8});
  CHECK(d.count(0xF8) - before == before);
}

namespace
{
//! A MIDI output device that notes when each message reaches it.
struct fake_midi_out final
    : ossia::net::protocol_base
    , ossia::net::midi::midi_stream
{
  fake_midi_out()
      : protocol_base{flags{}}
  {
  }
  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push(const ossia::net::parameter_base&, const ossia::value&) override
  {
    return false;
  }
  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base&) override { return false; }

  libremidi::midi_in* midi_in() const noexcept override { return nullptr; }
  void enable_registration() override { }
  void push_value(const libremidi::ump&) override { }
  void push_value(const libremidi::message& m) override
  {
    std::lock_guard l{mutex};
    sent.push_back(
        {std::chrono::steady_clock::now(), {m.bytes.begin(), m.bytes.end()}});
  }

  std::mutex mutex;
  std::vector<std::pair<std::chrono::steady_clock::time_point, std::vector<uint8_t>>>
      sent;
};
}

TEST_CASE("MIDI Sync Out: a MIDI device gets the score clock once, on time", "[midi_sync_out][.realtime]")
{
  auto proto = std::make_unique<fake_midi_out>();
  auto& midi = *proto;
  ossia::net::generic_device dev{std::move(proto), "fake"};

  driver d;
  d.fx.inputs.clock.value = mtk::MidiClockMode::Enabled;
  d.fx.inputs.clock_startstop.value = mtk::MidiStartStopMode::Enabled;
  d.fx.outputs.midi.ossia_node = &dev.get_root_node();

  // Ticks at the pace of a sound card, for 3 seconds.
  using namespace std::chrono;
  const auto t0 = steady_clock::now();
  const auto buffer = nanoseconds(int64_t(frames * 1e9 / rate));
  const int ticks = int(3. * rate / frames);
  for(int i = 0; i < ticks; i++)
  {
    std::this_thread::sleep_until(t0 + i * buffer);
    halp::tick_flicks tk{};
    tk.frames = frames;
    tk.tempo = 120.;
    tk.signature = {4, 4};
    const int64_t f0 = int64_t(i) * frames;
    tk.start_in_flicks = int64_t(f0 / rate * flicks_per_second);
    tk.end_in_flicks = int64_t((f0 + frames) / rate * flicks_per_second);
    tk.start_position_in_quarters = f0 / rate * 2.;
    tk.end_position_in_quarters = (f0 + frames) / rate * 2.;
    tk.position_in_frames = f0;
    d.fx.outputs.midi.midi_messages.clear();
    d.fx(tk);
    // Everything goes to the device: the port would write it a second time.
    CHECK(d.fx.outputs.midi.midi_messages.empty());
  }
  std::this_thread::sleep_for(milliseconds(100));

  std::vector<steady_clock::time_point> clocks;
  int starts = 0;
  {
    std::lock_guard l{midi.mutex};
    for(auto& [t, bytes] : midi.sent)
    {
      if(bytes == std::vector<uint8_t>{0xF8})
        clocks.push_back(t);
      if(bytes == std::vector<uint8_t>{0xFA})
        starts++;
    }
  }
  CHECK(starts == 1);
  // 48 clocks a second, each sent once.
  CHECK(int(clocks.size()) == Approx(ticks * frames / rate * 48.).margin(2));
  REQUIRE(clocks.size() > 100);

  // On a fixed grid: no drift over the run, little jitter from one to the next.
  const double period_ms = 1000. / 48.;
  const double span_ms
      = duration<double, std::milli>(clocks.back() - clocks.front()).count();
  CHECK(span_ms == Approx(period_ms * (clocks.size() - 1)).margin(2.));
  int late = 0;
  for(std::size_t i = 1; i < clocks.size(); i++)
  {
    const double dt = duration<double, std::milli>(clocks[i] - clocks[i - 1]).count();
    if(std::abs(dt - period_ms) > 2.)
      late++;
  }
  // The clock thread is not real time: a loaded machine delays a few.
  CHECK(late <= int(clocks.size()) / 20);
}

TEST_CASE("MIDI Sync Out: a positive clock offset delays the transport with the clock", "[midi_sync_out]")
{
  driver d;
  d.fx.inputs.clock.value = mtk::MidiClockMode::Enabled;
  d.fx.inputs.clock_startstop.value = mtk::MidiStartStopMode::Enabled;
  d.fx.inputs.clock_offset.value = 100.f;
  const int64_t offset = 4800;

  d.play(1.0);
  // Start one offset later, with the first clock.
  REQUIRE(d.dates(0xFA).size() == 1);
  CHECK(d.dates(0xFA).front() == offset);
  CHECK(d.dates(0xF8).front() == Approx(offset).margin(1));

  // A jump: Stop, Song Position and Continue one offset later, before the
  // first clock of the new position.
  const int64_t jump = d.now;
  d.play_from(49152, 0.5);
  const auto at = statuses(d.since(jump), jump + offset);
  CHECK(at == std::vector<uint8_t>{0xFC, 0xF2, 0xFB});
  int64_t first_clock = -1;
  for(auto& e : d.since(jump + offset))
    if(e.bytes[0] == 0xF8)
    {
      first_clock = e.at;
      break;
    }
  // The receiver resumes on the 16th note at 2.25 quarters, frame 54000.
  CHECK(first_clock == Approx(jump + offset + 54000 - 49152).margin(1));

  // Stop: nothing is left to wait for, it goes out after the last clock.
  d.stop();
  CHECK(d.count(0xFC) == 2);
  CHECK(d.dates(0xFC).back() >= d.dates(0xF8).back());

  // The port is in time order.
  for(std::size_t i = 1; i < d.out.size(); i++)
    CHECK(d.out[i - 1].at <= d.out[i].at);
}

TEST_CASE("MIDI Sync Out: a positive clock offset delays Start on a MIDI device too", "[midi_sync_out][.realtime]")
{
  auto proto = std::make_unique<fake_midi_out>();
  auto& midi = *proto;
  ossia::net::generic_device dev{std::move(proto), "fake"};

  driver d;
  d.fx.inputs.clock.value = mtk::MidiClockMode::Enabled;
  d.fx.inputs.clock_startstop.value = mtk::MidiStartStopMode::Enabled;
  d.fx.inputs.clock_offset.value = 100.f;
  d.fx.outputs.midi.ossia_node = &dev.get_root_node();

  using namespace std::chrono;
  const auto t0 = steady_clock::now();
  const auto buffer = nanoseconds(int64_t(frames * 1e9 / rate));
  const int ticks = int(1. * rate / frames);
  steady_clock::time_point first_tick{};
  for(int i = 0; i < ticks; i++)
  {
    std::this_thread::sleep_until(t0 + i * buffer);
    if(i == 0)
      first_tick = steady_clock::now();
    halp::tick_flicks tk{};
    tk.frames = frames;
    tk.tempo = 120.;
    tk.signature = {4, 4};
    const int64_t f0 = int64_t(i) * frames;
    tk.start_in_flicks = int64_t(f0 / rate * flicks_per_second);
    tk.end_in_flicks = int64_t((f0 + frames) / rate * flicks_per_second);
    tk.start_position_in_quarters = f0 / rate * 2.;
    tk.end_position_in_quarters = (f0 + frames) / rate * 2.;
    tk.position_in_frames = f0;
    d.fx.outputs.midi.midi_messages.clear();
    d.fx(tk);
    CHECK(d.fx.outputs.midi.midi_messages.empty());
  }
  d.stop();
  std::this_thread::sleep_for(milliseconds(250));

  steady_clock::time_point start{}, stop{}, first_clock{}, last_clock{};
  {
    std::lock_guard l{midi.mutex};
    for(auto& [t, bytes] : midi.sent)
    {
      if(bytes == std::vector<uint8_t>{0xFA})
        start = t;
      if(bytes == std::vector<uint8_t>{0xFC})
        stop = t;
      if(bytes == std::vector<uint8_t>{0xF8})
      {
        if(first_clock == steady_clock::time_point{})
          first_clock = t;
        last_clock = t;
      }
    }
  }
  REQUIRE(start != steady_clock::time_point{});
  REQUIRE(stop != steady_clock::time_point{});
  REQUIRE(first_clock != steady_clock::time_point{});
  const auto ms = [](auto d) { return duration<double, std::milli>(d).count(); };
  // One buffer of latency, then the offset.
  CHECK(ms(start - first_tick) == Approx(100. + 1000. * frames / rate).margin(5.));
  // As without an offset: the first clock with Start.
  CHECK(ms(first_clock - start) == Approx(0.).margin(3.));
  CHECK(stop >= last_clock);
}

TEST_CASE("MIDI Sync Out: the eight quarter frames carry one time", "[midi_sync_out]")
{
  driver d;
  d.fx.inputs.mtc.value = mtk::MidiTimeCodeMode::Enabled;
  d.fx.inputs.rate.value = mtk::MidiTimeCodeFrameRate::SMPTE_30;
  // From 14.99 s, across the carry to 16 s: the first timecode starts on the
  // next even frame, 15 s
  d.play_from(int64_t(14.99 * rate), 2.0);

  // Each full sequence of pieces 0 to 7, as a frame count
  std::vector<int64_t> times;
  std::array<int, 8> piece{};
  int expected = 0;
  for(auto& e : d.out)
  {
    if(e.bytes.size() != 2 || e.bytes[0] != 0xF1)
      continue;
    const int i = e.bytes[1] >> 4;
    if(i != expected)
    {
      expected = 0;
      if(i != 0)
        continue;
    }
    piece[i] = e.bytes[1] & 0x0F;
    expected = i + 1;
    if(i == 7)
    {
      const int f = piece[0] | ((piece[1] & 0x01) << 4);
      const int s = piece[2] | ((piece[3] & 0x03) << 4);
      const int m = piece[4] | ((piece[5] & 0x03) << 4);
      const int h = piece[6] | ((piece[7] & 0x01) << 4);
      times.push_back(((h * 60 + m) * 60 + s) * 30 + f);
      expected = 0;
    }
  }
  REQUIRE(times.size() > 20);
  CHECK(times.front() == 15 * 30);
  for(std::size_t i = 0; i < times.size(); i++)
  {
    INFO("timecode " << i << ": " << times[i] / 30 << " s " << times[i] % 30);
    CHECK(times[i] % 2 == 0);
    if(i > 0)
      CHECK(times[i] - times[i - 1] == 2);
  }
}

TEST_CASE("MIDI Sync Out: the free clock does not burst after a pause in the ticks", "[midi_sync_out]")
{
  driver d;
  d.fx.inputs.source.value = mtk::MidiClockSource::Free;
  d.fx.inputs.clock.value = mtk::MidiClockMode::Enabled;
  d.fx.inputs.clock_startstop.value = mtk::MidiStartStopMode::Enabled;
  d.fx.inputs.start_button.value.emplace();
  d.play(1.5 * frames / rate); // one tick
  d.fx.inputs.start_button.value.reset();

  // Not ticked for 300 ms, about 14 clocks at 120 BPM: the next tick only
  // forwards the clocks of the last buffers
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  const int64_t resumed = d.now;
  d.play(1.5 * frames / rate); // one tick
  int clocks = 0;
  for(auto& e : d.since(resumed))
    clocks += (e.bytes[0] == 0xF8);
  CHECK(clocks <= 2);
}

TEST_CASE("MIDI Sync Out: a stopped process lets go of its MIDI device", "[midi_sync_out]")
{
  auto proto = std::make_unique<fake_midi_out>();
  auto& midi = *proto;
  ossia::net::generic_device dev{std::move(proto), "fake"};
  const auto sent = [&] {
    std::lock_guard l{midi.mutex};
    return midi.sent.size();
  };

  driver d;
  d.fx.inputs.source.value = mtk::MidiClockSource::Free;
  d.fx.inputs.clock.value = mtk::MidiClockMode::Enabled;
  d.fx.inputs.clock_startstop.value = mtk::MidiStartStopMode::Enabled;
  d.fx.outputs.midi.ossia_node = &dev.get_root_node();
  d.fx.inputs.start_button.value.emplace();
  d.play(1.5 * frames / rate); // one tick
  d.fx.inputs.start_button.value.reset();

  // The free clock writes to the device while the process runs, ticked or not
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  REQUIRE(sent() > 2);

  // Once stopped, nothing reaches it: it may be removed before the next tick
  d.stop();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  const auto at_stop = sent();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  CHECK(sent() == at_stop);
}
