// The Arpeggiator presets of the default package: each one plays what its name
// says, and every note it starts, it stops.

#include "MidiPresetRig.hpp"

#include <Fx/Arpeggiator_v2.hpp>

#include <set>

using namespace midi_presets;

namespace
{
using Arp = Nodes::Arpeggiator::v2::Node;
using Rig = rig<Arp>;
const QString folder = "Arpeggiator";
const QString uuid = "4df48c1c-f5f2-4ab1-9f37-3b0adba139ae";

//! Loads `name` into a fresh rig.
void setup(Rig& r, const char* name)
{
  const auto errors = r.load(find(folder, uuid, name));
  INFO(errors.join("; ").toStdString());
  REQUIRE(errors.isEmpty());
}

//! C E G held from 0 to `until`.
Rig::input chord(int64_t until, int vel = 100)
{
  return {{0, on(60, vel)}, {0, on(64, vel)}, {0, on(67, vel)},
          {until, off(60)}, {until, off(64)}, {until, off(67)}};
}

//! Plays, releases, lets it end, and stops.
void finish(Rig& r, int64_t until, const Rig::input& in)
{
  r.run(until, in);
  r.stop();
  r.check_clean();
}

std::vector<int> first(std::vector<int> v, std::size_t n)
{
  v.resize(std::min(n, v.size()));
  return v;
}
}

TEST_CASE("Arpeggiator presets load", "[presets][arpeggiator]")
{
  const auto presets = load(folder, uuid);
  REQUIRE(presets.size() >= 12);
  for(const auto& p : presets)
  {
    INFO(p.name.toStdString());
    Arp a;
    const auto errors = apply_as_host(a.inputs, p);
    INFO(errors.join("; ").toStdString());
    CHECK(errors.isEmpty());
  }
}

TEST_CASE("Arpeggiator preset: Up 16th", "[presets][arpeggiator]")
{
  Rig r;
  setup(r, "Up 16th");
  const auto in = chord(bar);
  finish(r, 2 * bar, in);
  CHECK(first(r.on_pitches(), 7) == std::vector<int>{60, 64, 67, 60, 64, 67, 60});
  const auto dates = r.on_dates();
  REQUIRE(dates.size() == 16);
  for(std::size_t i = 0; i < dates.size(); i++)
    CHECK(dates[i] == int64_t(i) * sixteenth);
  for(auto& e : r.ons())
  {
    CHECK(e.data2 == 100);
    CHECK(e.channel == 1);
  }
  // Gate 100: a step lasts until the next one
  CHECK(r.end_of(60, 0) == sixteenth);
}

TEST_CASE("Arpeggiator preset: Up and down over 2 octaves", "[presets][arpeggiator]")
{
  Rig r;
  setup(r, "Up and down over 2 octaves");
  finish(r, 2 * bar, chord(2 * bar - sixteenth));
  const auto p = r.on_pitches();
  CHECK(std::set<int>(p.begin(), p.end()) == std::set<int>{60, 64, 67, 72, 76, 79});
}

TEST_CASE(
    "Arpeggiator preset: Up and down over 2 octaves, order",
    "[presets][arpeggiator]")
{
  // Up through both octaves, then down, the top and the bottom once each
  Rig r;
  setup(r, "Up and down over 2 octaves");
  finish(r, 2 * bar, chord(2 * bar - sixteenth));
  CHECK(
      first(r.on_pitches(), 11)
      == std::vector<int>{60, 64, 67, 72, 76, 79, 76, 72, 67, 64, 60});
}

TEST_CASE("Arpeggiator preset: Converge 16th", "[presets][arpeggiator]")
{
  Rig r;
  setup(r, "Converge 16th");
  const Rig::input in{{0, on(60)}, {0, on(64)}, {0, on(67)}, {0, on(72)},
                      {bar, off(60)}, {bar, off(64)}, {bar, off(67)}, {bar, off(72)}};
  finish(r, 2 * bar, in);
  CHECK(first(r.on_pitches(), 8) == std::vector<int>{60, 72, 64, 67, 60, 72, 64, 67});
}

TEST_CASE("Arpeggiator preset: Random seeded 16th", "[presets][arpeggiator]")
{
  auto take = [] {
    Rig r;
    setup(r, "Random seeded 16th");
    finish(r, 3 * bar, chord(2 * bar));
    return r.on_pitches();
  };
  const auto a = take();
  const auto b = take();
  REQUIRE(a.size() == 32);
  CHECK(a == b);
  CHECK(std::set<int>(a.begin(), a.end()) == std::set<int>{60, 64, 67});
  std::vector<int> up;
  for(int i = 0; i < 32; i++)
    up.push_back(std::array{60, 64, 67}[i % 3]);
  CHECK(a != up);

  // The same sequence again after a stop
  Rig r;
  setup(r, "Random seeded 16th");
  finish(r, bar, chord(bar - 1));
  const auto before = r.on_pitches();
  r.events.clear();
  const int64_t t0 = r.pos;
  const Rig::input again{{t0, on(60)}, {t0, on(64)}, {t0, on(67)},
                         {t0 + bar - 1, off(60)}, {t0 + bar - 1, off(64)},
                         {t0 + bar - 1, off(67)}};
  finish(r, t0 + bar, again);
  CHECK(r.on_pitches() == before);
}

TEST_CASE("Arpeggiator preset: Swung 16th", "[presets][arpeggiator]")
{
  Rig r;
  setup(r, "Swung 16th");
  finish(r, 2 * bar, chord(bar));
  const auto d = r.on_dates();
  REQUIRE(d.size() >= 8);
  // The off-beat sixteenths move to the triplet: a third of a step late
  for(std::size_t i = 0; i < 8; i++)
    CHECK(d[i] == int64_t(i) * sixteenth + (i % 2 ? sixteenth / 3 : 0));
}

TEST_CASE("Arpeggiator preset: Staccato 16th", "[presets][arpeggiator]")
{
  Rig r;
  setup(r, "Staccato 16th");
  finish(r, 2 * bar, chord(bar));
  const auto ons = r.ons();
  REQUIRE(ons.size() == 16);
  for(auto& e : ons)
  {
    INFO("step at " << e.at);
    CHECK(r.end_of(e.data1, e.at) == e.at + sixteenth * 30 / 100);
  }
}

TEST_CASE("Arpeggiator preset: Legato 8th", "[presets][arpeggiator]")
{
  Rig r;
  setup(r, "Legato 8th");
  finish(r, 2 * bar, chord(bar));
  const auto ons = r.ons();
  REQUIRE(ons.size() == 8);
  for(std::size_t i = 0; i < ons.size(); i++)
  {
    INFO("step " << i);
    CHECK(ons[i].at == int64_t(i) * 2 * sixteenth);
    // Each step overlaps the next by half a step
    if(i + 1 < ons.size() - 1)
      CHECK(r.end_of(ons[i].data1, ons[i].at) == ons[i].at + 3 * sixteenth);
  }
}

TEST_CASE("Arpeggiator preset: Accent every 4 steps", "[presets][arpeggiator]")
{
  Rig r;
  setup(r, "Accent every 4 steps");
  finish(r, 2 * bar, chord(bar, 80));
  const auto ons = r.ons();
  REQUIRE(ons.size() == 16);
  for(std::size_t i = 0; i < ons.size(); i++)
  {
    INFO("step " << i);
    CHECK(ons[i].data2 == (i % 4 == 0 ? 127 : 80));
  }
}

TEST_CASE("Arpeggiator preset: Trance gate chord 16th", "[presets][arpeggiator]")
{
  Rig r;
  setup(r, "Trance gate chord 16th");
  finish(r, 2 * bar, chord(bar));
  const auto ons = r.ons();
  REQUIRE(ons.size() == 48);
  for(std::size_t i = 0; i < ons.size(); i++)
  {
    INFO("note " << i);
    CHECK(ons[i].at == int64_t(i / 3) * sixteenth);
    CHECK(r.end_of(ons[i].data1, ons[i].at) == ons[i].at + sixteenth / 2);
  }
}

TEST_CASE("Arpeggiator preset: Note repeat 16th with pressure", "[presets][arpeggiator]")
{
  SECTION("Channel pressure")
  {
    Rig r;
    setup(r, "Note repeat 16th with pressure");
    const Rig::input in{
        {0, on(60, 100)},
        {sixteenth * 4 - 100, libremidi::channel_events::aftertouch(1, 40)},
        {sixteenth * 8 - 100, libremidi::channel_events::aftertouch(1, 110)},
        {sixteenth * 12 - 100, off(60)}};
    finish(r, bar, in);
    const auto ons = r.ons();
    REQUIRE(ons.size() == 12);
    for(std::size_t i = 0; i < ons.size(); i++)
    {
      INFO("step " << i);
      CHECK(ons[i].data1 == 60);
      CHECK(ons[i].at == int64_t(i) * sixteenth);
      CHECK(ons[i].data2 == (i < 4 ? 100 : i < 8 ? 40 : 110));
      CHECK(r.end_of(60, ons[i].at) == ons[i].at + sixteenth / 2);
    }
  }
  SECTION("Poly pressure, per pad")
  {
    Rig r;
    setup(r, "Note repeat 16th with pressure");
    const Rig::input in{
        {0, on(60, 100)},
        {0, on(64, 100)},
        {sixteenth * 2 - 100, libremidi::channel_events::poly_pressure(1, 64, 30)},
        {sixteenth * 4 - 100, off(60)},
        {sixteenth * 4 - 100, off(64)}};
    finish(r, bar, in);
    for(auto& e : r.ons())
    {
      INFO("pitch " << e.data1 << " at " << e.at);
      CHECK(e.data2 == (e.data1 == 64 && e.at >= 2 * sixteenth ? 30 : 100));
    }
    CHECK(r.ons().size() == 8);
  }
}

TEST_CASE("Arpeggiator preset: Note repeat 32nd triplet roll", "[presets][arpeggiator]")
{
  Rig r;
  setup(r, "Note repeat 32nd triplet roll");
  finish(r, bar, {{0, on(38, 90)}, {quarter - 100, off(38)}});
  const auto ons = r.ons();
  const int64_t step = quarter / 12;
  REQUIRE(ons.size() == 12);
  for(std::size_t i = 0; i < ons.size(); i++)
  {
    INFO("step " << i);
    CHECK(ons[i].at == int64_t(i) * step);
    CHECK(ons[i].data2 == 90);
    CHECK(r.end_of(38, ons[i].at) == ons[i].at + step / 2);
  }
}

TEST_CASE("Arpeggiator preset: Latched pad arp", "[presets][arpeggiator]")
{
  Rig r;
  setup(r, "Latched pad arp");
  const int64_t eighth = 2 * sixteenth;
  // A chord tapped for a beat keeps playing; the next chord replaces it
  const Rig::input in{
      {0, on(60)},
      {0, on(64)},
      {0, on(67)},
      {quarter - 100, off(60)},
      {quarter - 100, off(64)},
      {quarter - 100, off(67)},
      {2 * bar, on(62)},
      {2 * bar, on(65)},
      {2 * bar + quarter - 100, off(62)},
      {2 * bar + quarter - 100, off(65)},
      {4 * bar, cc(123, 0)}};
  r.run(5 * bar, in);
  r.stop();
  r.check_clean();

  std::set<int> first_chord, second_chord;
  for(auto& e : r.ons())
  {
    if(e.at < 2 * bar)
      first_chord.insert(e.data1);
    else if(e.at < 4 * bar)
      second_chord.insert(e.data1);
    else
      FAIL("a note after all notes off, at " << e.at);
  }
  CHECK(first_chord == std::set<int>{60, 64, 67, 72, 76, 79});
  CHECK(second_chord == std::set<int>{62, 65, 74, 77});
  // Steps every eighth, through the whole time the chords are latched
  const auto d = r.on_dates();
  CHECK(d.size() == 32);
  for(std::size_t i = 0; i < d.size(); i++)
    CHECK(d[i] == int64_t(i) * eighth);
}
