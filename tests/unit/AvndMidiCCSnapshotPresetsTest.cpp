// The Midi CC snapshot presets of the default package load onto the object
// and send what their names say.

#include "MidiPresetHarness.hpp"

#include <Advanced/MidiScaler/MidiCCSnapshot.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

using S = mtk::MidiCCSnapshot;

namespace
{
using bytes = std::vector<int>;
using list = std::vector<bytes>;

bytes cc(int ch, int n, int v) { return {0xB0 + ch - 1, n, v}; }

const std::vector<midi_presets::preset>& all()
{
  static const auto p
      = midi_presets::load("Midi CC snapshot", "e941cff0-8ebb-4259-a146-29ce10125791");
  return p;
}

struct driver
{
  S fx;
  list out;

  explicit driver(const midi_presets::preset& p)
  {
    const auto errors = midi_presets::apply(fx.inputs, p);
    INFO(errors.join("; ").toStdString());
    REQUIRE(errors.isEmpty());
  }
  explicit driver(const char* name)
  {
    const midi_presets::preset* found{};
    for(auto& p : all())
      if(p.name == name)
        found = &p;
    INFO(name);
    REQUIRE(found);
    const auto errors = midi_presets::apply(fx.inputs, *found);
    INFO(errors.join("; ").toStdString());
    REQUIRE(errors.isEmpty());
  }

  void run(const list& in, bool send = false, int frames = 64)
  {
    out.clear();
    int ts = 0;
    for(auto& b : in)
    {
      libremidi::message m;
      for(int x : b)
        m.bytes.push_back(uint8_t(x));
      m.timestamp = ts++;
      fx.inputs.midi.midi_messages.push_back(m);
    }
    if(send)
      fx.inputs.send.value = halp::impulse_type{};
    fx(halp::tick_musical{.frames = frames});
    fx.inputs.send.value.reset();
    for(auto& m : fx.outputs.midi)
    {
      CHECK(m.timestamp >= 0);
      CHECK(m.timestamp < frames);
      out.push_back(bytes(m.bytes.begin(), m.bytes.end()));
    }
    fx.outputs.midi.midi_messages.clear();
    fx.inputs.midi.midi_messages.clear();
  }
};

//! The (controller, value) pairs of the active slots of a preset.
list expected(const midi_presets::preset& p)
{
  int channel = 1, count = 8;
  int slot[16]{};
  for(auto e : p.values)
  {
    const auto pair = e.toArray();
    const int idx = pair[0].toInt();
    const int v = pair[1].toObject()["Int"].toInt();
    if(idx == 4)
      channel = v;
    else if(idx == 5)
      count = v;
    else if(idx >= 6 && idx < 22)
      slot[idx - 6] = v;
  }
  list res;
  for(int i = 0; i < count; i++)
    res.push_back(cc(channel, slot[2 * i], slot[2 * i + 1]));
  return res;
}
}

TEST_CASE("Midi CC snapshot presets send their slots", "[presets][midi_cc_snapshot]")
{
  REQUIRE(all().size() >= 6);
  for(auto& p : all())
  {
    INFO(p.name.toStdString());
    CHECK(!p.category.isEmpty());
    driver d{p};
    // The first tick only records the slots
    d.run({});
    CHECK(d.out.empty());
    d.run({{0x90, 60, 100}}, true);
    auto exp = expected(p);
    exp.push_back({0x90, 60, 100});
    CHECK(d.out == exp);
  }
}

TEST_CASE("Midi CC snapshot preset contents", "[presets][midi_cc_snapshot]")
{
  {
    driver d{"All controllers reset"};
    d.run({}, true);
    CHECK(d.out == list{cc(1, 121, 0)});
  }
  {
    driver d{"Controllers reset and notes off"};
    d.run({}, true);
    CHECK(d.out == list{cc(1, 121, 0), cc(1, 123, 0)});
  }
  {
    driver d{"Volume pan expression mod"};
    d.run({}, true);
    CHECK(d.out == list{cc(1, 7, 100), cc(1, 10, 64), cc(1, 11, 127), cc(1, 1, 0)});
  }
  {
    driver d{"Drum channel init"};
    d.run({}, true);
    REQUIRE(!d.out.empty());
    for(auto& m : d.out)
      CHECK(m[0] == 0xB9);
  }
  {
    driver d{"Mixer scene muted"};
    d.run({}, true);
    CHECK(d.out == list{cc(1, 7, 0)});
  }
  {
    driver d{"Pedals up"};
    d.run({}, true);
    CHECK(d.out == list{cc(1, 64, 0), cc(1, 65, 0), cc(1, 66, 0), cc(1, 67, 0)});
  }
}

TEST_CASE("Midi CC snapshot preset: knob takeover", "[presets][midi_cc_snapshot]")
{
  driver d{"Knob takeover CC 20-27"};
  d.run({});
  d.run({}, true);
  CHECK(d.out.size() == 8);

  // The knob is far below the recalled 64: held back until it crosses it
  d.run({cc(1, 20, 10), cc(1, 20, 40), cc(1, 21, 100)});
  CHECK(d.out.empty());
  d.run({cc(1, 20, 70), cc(1, 20, 71)});
  CHECK(d.out == list{cc(1, 20, 70), cc(1, 20, 71)});

  // Other channels are not held
  d.run({cc(2, 22, 1)});
  CHECK(d.out == list{cc(2, 22, 1)});

  // Send on change: editing a slot sends it
  d.fx.inputs.value3.value = 90;
  d.run({});
  CHECK(d.out == list{cc(1, 22, 90)});
}

TEST_CASE("Midi CC snapshot preset: fader takeover", "[presets][midi_cc_snapshot]")
{
  driver d{"Fader takeover volume and pan"};
  d.run({});
  d.run({}, true);
  CHECK(d.out == list{cc(1, 7, 100), cc(1, 10, 64)});
  // From above: held until it reaches the stored value
  d.run({cc(1, 7, 127), cc(1, 7, 110), cc(1, 7, 100), cc(1, 7, 90)});
  CHECK(d.out == list{cc(1, 7, 100), cc(1, 7, 90)});
  // Not send on change
  d.fx.inputs.value1.value = 50;
  d.run({});
  CHECK(d.out.empty());
}
