// The Midi quantify presets of the default package: each one plays what its
// name says, and every note it starts, it stops.

#include "MidiPresetRig.hpp"

#include <Fx/Quantifier_v2.hpp>

using namespace midi_presets;

namespace
{
using Quantify = Nodes::Quantifier::v2::Node;
using Rig = rig<Quantify, halp::tick_flicks>;
const QString folder = "Midi quantify";
const QString uuid = "a0cb1144-05a8-4f9b-b22c-81cd964314fb";

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

struct note
{
  int pitch;
  int64_t on, off;
  int vel{100};
};
}

TEST_CASE("Midi quantify presets load", "[presets][quantify]")
{
  const auto presets = load(folder, uuid);
  REQUIRE(presets.size() >= 7);
  for(const auto& p : presets)
  {
    INFO(p.name.toStdString());
    Quantify q;
    const auto errors = apply_as_host(q.inputs, p);
    INFO(errors.join("; ").toStdString());
    CHECK(errors.isEmpty());
  }
}

TEST_CASE("Midi quantify preset: Tight 16th", "[presets][quantify]")
{
  Rig r;
  setup(r, "Tight 16th");
  // Late by 100 frames: the next sixteenth. Released before it starts: it
  // keeps the length it was played with.
  finish(
      r, bar,
      {{3100, on(60)}, {5000, off(60)}, {6100, on(62)}, {11000, off(62)},
       {17000, on(64)}, {30000, off(64)}});
  const auto ons = r.ons();
  REQUIRE(ons.size() == 3);
  CHECK(ons[0].at == 6000);
  CHECK(r.end_of(60, 6000) == 6000 + 1900);
  CHECK(ons[1].at == 12000);
  CHECK(r.end_of(62, 12000) == 12000 + 4900);
  // Held past its start: until its release
  CHECK(ons[2].at == 18000);
  CHECK(r.end_of(64, 18000) == 30000);
}

TEST_CASE(
    "Midi quantify preset: Tight 16th, a note on the grid", "[presets][quantify]")
{
  // A note exactly on a grid point is on time: it starts at once
  Rig r;
  setup(r, "Tight 16th");
  finish(r, bar, {{18000, on(64)}, {20000, off(64)}, {24000, on(65)}, {26000, off(65)}});
  CHECK(r.on_dates() == std::vector<int64_t>{18000, 24000});
}

TEST_CASE("Midi quantify preset: Loose 8th", "[presets][quantify]")
{
  Rig r;
  setup(r, "Loose 8th");
  // Tightness 0.5: up to a quarter of an eighth late counts as on time
  finish(
      r, bar,
      {{14000, on(60)}, {15000, off(60)}, {16000, on(62)}, {17000, off(62)},
       {30000, on(64)}, {31000, off(64)}});
  const auto ons = r.ons();
  REQUIRE(ons.size() == 3);
  CHECK(ons[0].at == 14000);
  CHECK(ons[1].at == 24000);
  CHECK(ons[2].at == 36000);
}

TEST_CASE("Midi quantify preset: Fixed 16th notes", "[presets][quantify]")
{
  Rig r;
  setup(r, "Fixed 16th notes");
  finish(r, bar, {{100, on(60)}, {20000, off(60)}, {23000, on(62)}, {23500, off(62)}});
  const auto ons = r.ons();
  REQUIRE(ons.size() == 2);
  CHECK(ons[0].at == 6000);
  CHECK(r.end_of(60, 6000) == 12000);
  CHECK(ons[1].at == 24000);
  CHECK(r.end_of(62, 24000) == 30000);
  CHECK(r.offs().size() == 2);
}

TEST_CASE("Midi quantify preset: Staccato 16th", "[presets][quantify]")
{
  Rig r;
  setup(r, "Staccato 16th");
  finish(r, bar, {{3000, on(60)}, {18000, off(60)}, {23000, on(62)}, {23100, off(62)}});
  const auto ons = r.ons();
  REQUIRE(ons.size() == 2);
  CHECK(ons[0].at == 6000);
  CHECK(r.end_of(60, 6000) == 9000);
  CHECK(ons[1].at == 24000);
  CHECK(r.end_of(62, 24000) == 27000);
}

TEST_CASE("Midi quantify preset: Notes end on the beat", "[presets][quantify]")
{
  Rig r;
  setup(r, "Notes end on the beat");
  finish(r, 2 * bar, {{3000, on(60)}, {4000, off(60)}, {29000, on(62)}, {70000, off(62)}});
  const auto ons = r.ons();
  REQUIRE(ons.size() == 2);
  CHECK(ons[0].at == 6000);
  CHECK(r.end_of(60, 6000) == quarter);
  CHECK(ons[1].at == 30000);
  CHECK(r.end_of(62, 30000) == 2 * quarter);
}

TEST_CASE("Midi quantify preset: Play on release with decay", "[presets][quantify]")
{
  Rig r;
  setup(r, "Play on release with decay");
  // The note plays when the key comes up, as long as it was held, its
  // velocity lowered by 20 percent, its note-off at the release velocity. A
  // second release while it sounds ends it and lowers the velocity again.
  finish(
      r, bar,
      {{1000, on(60, 100)}, {5000, off(60, 64)}, {7000, on(60, 100)}, {9000, off(60, 50)}});
  const auto ons = r.ons();
  REQUIRE(ons.size() == 2);
  CHECK(ons[0].at == 6000);
  CHECK(ons[0].data2 == 80);
  CHECK(ons[1].at == 12000);
  CHECK(ons[1].data2 == 64);
  const auto offs = r.offs();
  REQUIRE(offs.size() == 2);
  CHECK(offs[0].at == 9000);
  CHECK(offs[0].data2 == 64);
  CHECK(offs[1].at == 14000);
  CHECK(offs[1].data2 == 50);
}

TEST_CASE("Midi quantify preset: Fixed length without grid", "[presets][quantify]")
{
  Rig r;
  setup(r, "Fixed length without grid");
  finish(r, bar, {{1234, on(60)}, {1300, off(60)}, {20000, on(62)}, {40000, off(62)}});
  const auto ons = r.ons();
  REQUIRE(ons.size() == 2);
  CHECK(ons[0].at == 1234);
  CHECK(r.end_of(60, 1234) == 1234 + sixteenth);
  CHECK(ons[1].at == 20000);
  CHECK(r.end_of(62, 20000) == 20000 + sixteenth);
}
