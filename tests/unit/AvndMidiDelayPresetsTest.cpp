// The Midi delay presets of the default package: each one plays what its name
// says, and every note it starts, it stops.

#include "MidiPresetRig.hpp"

#include <Advanced/MidiScaler/MidiDelay.hpp>

#include <cmath>

using namespace midi_presets;

namespace
{
using Delay = mtk::MidiDelay;
using Rig = rig<Delay>;
const QString folder = "Midi delay";
const QString uuid = "5ba4e20c-8936-43ab-aee5-8ac4555e05c1";

void setup(Rig& r, const char* name)
{
  const auto errors = r.load(find(folder, uuid, name));
  INFO(errors.join("; ").toStdString());
  REQUIRE(errors.isEmpty());
}

void finish(Rig& r, int64_t until, const Rig::input& in)
{
  r.run(until, in);
  r.stop();
  r.check_clean();
}

struct expected
{
  int64_t at;
  int pitch;
  int vel;
};

//! The note-ons, and each one's end `length` later.
void check_notes(const Rig& r, const std::vector<expected>& want, int64_t length, int chan = 1)
{
  const auto ons = r.ons();
  REQUIRE(ons.size() == want.size());
  for(std::size_t i = 0; i < want.size(); i++)
  {
    INFO("note " << i);
    CHECK(ons[i].at == want[i].at);
    CHECK(ons[i].data1 == want[i].pitch);
    CHECK(ons[i].data2 == want[i].vel);
    CHECK(ons[i].channel == chan);
    CHECK(r.end_of(want[i].pitch, want[i].at, chan) == want[i].at + length);
  }
}

int vel(double v, double fb, int k)
{
  return int(std::lround(v * std::pow(fb, k)));
}
}

TEST_CASE("Midi delay presets load", "[presets][midi_delay]")
{
  const auto presets = load(folder, uuid);
  REQUIRE(presets.size() >= 9);
  for(const auto& p : presets)
  {
    INFO(p.name.toStdString());
    Delay d;
    const auto errors = apply_as_host(d.inputs, p);
    INFO(errors.join("; ").toStdString());
    CHECK(errors.isEmpty());
  }
}

TEST_CASE("Midi delay preset: Dotted eighth echo", "[presets][midi_delay]")
{
  Rig r;
  setup(r, "Dotted eighth echo");
  finish(r, 2 * bar, {{0, on(60)}, {3000, off(60)}});
  const int64_t t = 3 * sixteenth;
  std::vector<expected> want{{0, 60, 100}};
  for(int k = 1; k <= 4; k++)
    want.push_back({k * t, 60, vel(100, 0.7, k)});
  check_notes(r, want, 3000);
}

TEST_CASE("Midi delay preset: Quarter note echo", "[presets][midi_delay]")
{
  Rig r;
  setup(r, "Quarter note echo");
  finish(r, 2 * bar, {{0, on(60)}, {3000, off(60)}});
  std::vector<expected> want{{0, 60, 100}};
  for(int k = 1; k <= 3; k++)
    want.push_back({k * quarter, 60, vel(100, 0.6, k)});
  check_notes(r, want, 3000);
}

TEST_CASE("Midi delay preset: Slapback 80 ms", "[presets][midi_delay]")
{
  Rig r;
  setup(r, "Slapback 80 ms");
  finish(r, bar, {{0, on(60)}, {2000, off(60)}});
  // 80 ms whatever the tempo
  check_notes(r, {{0, 60, 100}, {3840, 60, 70}}, 2000);
}

TEST_CASE("Midi delay preset: Wet only ping", "[presets][midi_delay]")
{
  Rig r;
  setup(r, "Wet only ping");
  finish(r, 2 * bar, {{0, on(60)}, {3000, off(60)}});
  std::vector<expected> want;
  for(int k = 1; k <= 3; k++)
    want.push_back({k * 2 * sixteenth, 60, vel(100, 0.7, k)});
  check_notes(r, want, 3000);
}

TEST_CASE("Midi delay preset: Rising fifths cascade", "[presets][midi_delay]")
{
  Rig r;
  setup(r, "Rising fifths cascade");
  finish(r, 2 * bar, {{0, on(60)}, {3000, off(60)}});
  std::vector<expected> want{{0, 60, 100}};
  for(int k = 1; k <= 4; k++)
    want.push_back({k * 2 * sixteenth, 60 + 7 * k, vel(100, 0.85, k)});
  check_notes(r, want, 3000);
}

TEST_CASE("Midi delay preset: Falling octaves", "[presets][midi_delay]")
{
  Rig r;
  setup(r, "Falling octaves");
  finish(r, 2 * bar, {{0, on(60)}, {3000, off(60)}});
  std::vector<expected> want{{0, 60, 100}};
  for(int k = 1; k <= 3; k++)
    want.push_back({k * quarter, 60 - 12 * k, vel(100, 0.8, k)});
  check_notes(r, want, 3000);
}

TEST_CASE("Midi delay preset: Infinite ostinato", "[presets][midi_delay]")
{
  Rig r;
  setup(r, "Infinite ostinato");
  // Every bar, at full velocity, until all notes off
  finish(r, 7 * bar, {{0, on(48)}, {quarter, off(48)}, {5 * bar + 100, cc(123, 0)}});
  const auto ons = r.ons();
  REQUIRE(ons.size() == 6);
  for(int k = 0; k <= 5; k++)
  {
    INFO("echo " << k);
    CHECK(ons[k].at == k * bar);
    CHECK(ons[k].data2 == 100);
    CHECK(r.end_of(48, k * bar) == (k < 5 ? k * bar + quarter : 5 * bar + 100));
  }
}

TEST_CASE("Midi delay preset: Short ratchet", "[presets][midi_delay]")
{
  Rig r;
  setup(r, "Short ratchet");
  finish(r, bar, {{0, on(38)}, {1000, off(38)}});
  // 32nd-note repeats, each a 64th long whatever the source's length
  const int64_t t = sixteenth / 2;
  const auto ons = r.ons();
  REQUIRE(ons.size() == 5);
  CHECK(r.end_of(38, 0) == 1000);
  for(int k = 1; k <= 4; k++)
  {
    INFO("echo " << k);
    CHECK(ons[k].at == k * t);
    CHECK(ons[k].data2 == vel(100, 0.9, k));
    CHECK(r.end_of(38, k * t) == k * t + t / 2);
  }
}

TEST_CASE("Midi delay preset: MPE echo", "[presets][midi_delay]")
{
  Rig r;
  setup(r, "MPE echo");
  // A note on its own channel, bent while held: each echo is bent the same
  // way on that channel, the same time after its start
  finish(
      r, 2 * bar,
      {{0, on(60, 100, 3)},
       {2000, libremidi::channel_events::pitch_bend(3, 12000)},
       {2500, libremidi::channel_events::aftertouch(3, 77)},
       {6000, off(60, 0, 3)}});
  std::vector<expected> want{{0, 60, 100}};
  for(int k = 1; k <= 3; k++)
    want.push_back({k * 2 * sixteenth, 60, vel(100, 0.7, k)});
  check_notes(r, want, 6000, 3);

  std::vector<int64_t> bends, pressures;
  for(auto& e : r.events)
  {
    if(e.status == 0xE0)
    {
      CHECK(e.channel == 3);
      CHECK((e.data1 | (e.data2 << 7)) == 12000);
      bends.push_back(e.at);
    }
    else if(e.status == 0xD0)
    {
      CHECK(e.channel == 3);
      CHECK(e.data1 == 77);
      pressures.push_back(e.at);
    }
  }
  CHECK(bends == std::vector<int64_t>{2000, 14000, 26000, 38000});
  CHECK(pressures == std::vector<int64_t>{2500, 14500, 26500, 38500});
}
