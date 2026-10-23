// The MIDI Sync Out presets of the default package load onto the object and
// produce, on the audio path, the clock, transport and timecode they are named
// for. What the clock thread sends straight to a MIDI device is not covered:
// with no device the score clock thread sends nothing, and the free clock only
// runs once its Start button is pressed.

#include "MidiPresetHarness.hpp"

#include <AvndProcesses/MIDISync.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <map>
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
  void play(double seconds, double tempo = 120.)
  {
    const int64_t n = int64_t(seconds * rate / frames);
    for(int64_t i = 0; i < n; i++)
    {
      halp::tick_flicks tk{};
      tk.frames = frames;
      tk.tempo = tempo;
      tk.signature = {4, 4};
      const int64_t f0 = i * frames;
      const int64_t f1 = f0 + frames;
      tk.start_in_flicks = int64_t(f0 / rate * flicks_per_second);
      tk.end_in_flicks = int64_t(f1 / rate * flicks_per_second);
      tk.start_position_in_quarters = f0 / rate * tempo / 60.;
      tk.end_position_in_quarters = f1 / rate * tempo / 60.;
      tk.relative_position = double(i) / 1000.;
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

  double seconds() const { return now / rate; }

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
