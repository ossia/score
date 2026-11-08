/**
 * Tests for the Open Fixture Library reader and for the instantiation of its
 * fixtures as DMX parameters.
 *
 * Expected values come from the fixture format specification
 * (docs/fixture-format.md in the open-fixture-library repository) and from the
 * DMX byte layout they imply, never from a run of the code.
 */

#include <Protocols/Artnet/ArtnetSpecificSettings.hpp>
#include <Protocols/Artnet/DMXFixtureInstantiation.hpp>
#include <Protocols/Artnet/OpenFixtureLibrary.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/StdVariantSerialization.hpp>
#include <score/serialization/VariantSerialization.hpp>

#include <ossia/detail/fmt.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>
#include <ossia/protocols/artnet/dmx_buffer.hpp>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>

#include <string>

using namespace Protocols;

namespace
{
std::string fixture(const std::string& body)
{
  return R"_({"$schema":"https://raw.githubusercontent.com/OpenLightingProject/open-fixture-library/master/schemas/fixture.json","name":"Test",)_"
         + body + "}";
}

OFL::FixtureDefinition parse(const std::string& body)
{
  auto def = OFL::parseFixture(fixture(body));
  REQUIRE(def.has_value());
  return *def;
}

const OFL::FixtureMode& onlyMode(const OFL::FixtureDefinition& def)
{
  REQUIRE(def.modes.size() == 1);
  return def.modes.front();
}

struct Rig
{
  ossia::net::generic_device dev{
      std::make_unique<ossia::net::multiplex_protocol>(), "dmx"};
  ossia::net::dmx_buffer buffer{512};

  Rig() { buffer.set_universe_count(1); }

  void
  add(const OFL::FixtureMode& mode, int address = 0, int universe = 0,
      int start_universe = 0)
  {
    Artnet::Fixture f;
    f.fixtureName = "F";
    f.modeName = mode.name;
    f.mode.channelNames = mode.allChannels;
    f.controls = mode.channels;
    f.address = address;
    f.universe = universe;
    addArtnetFixture(dev, buffer, f, start_universe, 512);
  }

  ossia::net::node_base* node(std::string_view path)
  {
    return ossia::net::find_node(dev.get_root_node(), path);
  }

  ossia::net::parameter_base& param(std::string_view path)
  {
    auto n = node(path);
    REQUIRE(n);
    REQUIRE(n->get_parameter());
    return *n->get_parameter();
  }

  int max(std::string_view path)
  {
    return ossia::convert<int>(ossia::get_max(param(path).get_domain()));
  }

  int at(int channel) const { return buffer.data[channel]; }
};

const std::string panTiltFine = R"_(
  "availableChannels": {
    "Pan": {"fineChannelAliases": ["Pan fine"], "capability": {"type": "Pan", "angleStart": "0deg", "angleEnd": "540deg"}},
    "Tilt": {"fineChannelAliases": ["Tilt fine"], "capability": {"type": "Tilt", "angleStart": "0deg", "angleEnd": "270deg"}}
  },
  "modes": [{"name": "Interleaved", "channels": ["Pan", "Tilt", "Pan fine", "Tilt fine"]}]
)_";
}

TEST_CASE("A 16-bit channel spans the whole 16-bit range", "[dmx][fixture]")
{
  Rig rig;
  rig.add(onlyMode(parse(panTiltFine)));
  CHECK(rig.max("/F/Pan") == 65535);
  CHECK(rig.max("/F/Tilt") == 65535);
}

TEST_CASE("Fine bytes go to the fine channel's own slot", "[dmx][fixture]")
{
  Rig rig;
  rig.add(onlyMode(parse(panTiltFine)));

  rig.param("/F/Pan").push_value(0x1234);
  rig.param("/F/Tilt").push_value(0xABCD);

  CHECK(rig.at(0) == 0x12);
  CHECK(rig.at(1) == 0xAB);
  CHECK(rig.at(2) == 0x34);
  CHECK(rig.at(3) == 0xCD);
}

TEST_CASE("A mode without the fine channel drives the coarse one only", "[dmx][fixture]")
{
  auto def = parse(R"_(
    "availableChannels": {
      "Dimmer": {"fineChannelAliases": ["Dimmer fine"], "defaultValue": 65535, "capability": {"type": "Intensity"}},
      "Red": {"capability": {"type": "ColorIntensity", "color": "Red"}}
    },
    "modes": [{"name": "8bit", "channels": ["Dimmer", "Red"]}]
  )_");
  Rig rig;
  rig.add(onlyMode(def));

  CHECK(rig.max("/F/Dimmer") == 255);
  CHECK(rig.at(0) == 255);
  rig.param("/F/Dimmer").push_value(10);
  CHECK(rig.at(0) == 10);
  CHECK(rig.at(1) == 0);
}

TEST_CASE(
    "A channel with capability ranges and a fine channel is 16-bit", "[dmx][fixture]")
{
  auto def = parse(R"_(
    "availableChannels": {
      "Strobe": {
        "fineChannelAliases": ["Strobe fine"],
        "capabilities": [
          {"dmxRange": [0, 32767], "type": "ShutterStrobe", "shutterEffect": "Open"},
          {"dmxRange": [32768, 65535], "type": "ShutterStrobe", "shutterEffect": "Strobe", "speedStart": "1Hz", "speedEnd": "20Hz"}
        ]
      }
    },
    "modes": [{"name": "16bit", "channels": ["Strobe", "Strobe fine"]}]
  )_");
  Rig rig;
  rig.add(onlyMode(def));

  CHECK(rig.max("/F/Strobe") == 65535);
  rig.param("/F/Strobe/preset").push_value(std::string("Strobe 1Hz to 20Hz"));
  CHECK(rig.at(0) == 0x80);
  CHECK(rig.at(1) == 0x00);
}

TEST_CASE("8-bit ranges of a 16-bit channel are scaled to it", "[dmx][fixture]")
{
  auto def = parse(R"_(
    "availableChannels": {
      "Gobo": {
        "fineChannelAliases": ["Gobo fine"],
        "dmxValueResolution": "8bit",
        "capabilities": [
          {"dmxRange": [0, 127], "type": "Effect", "effectName": "A"},
          {"dmxRange": [128, 255], "type": "Effect", "effectName": "B"}
        ]
      }
    },
    "modes": [{"name": "m", "channels": ["Gobo", "Gobo fine"]}]
  )_");
  Rig rig;
  rig.add(onlyMode(def));

  rig.param("/F/Gobo/preset").push_value(std::string("B"));
  CHECK(rig.at(0) == 128);
  CHECK(rig.at(1) == 0);
}

TEST_CASE("The default preset is the range holding the default value", "[dmx][fixture]")
{
  auto def = parse(R"_(
    "availableChannels": {
      "Strobe": {
        "defaultValue": 255,
        "capabilities": [
          {"dmxRange": [0, 247], "type": "ShutterStrobe", "shutterEffect": "Strobe"},
          {"dmxRange": [248, 255], "type": "ShutterStrobe", "shutterEffect": "Open"}
        ]
      }
    },
    "modes": [{"name": "m", "channels": ["Strobe"]}]
  )_");
  Rig rig;
  rig.add(onlyMode(def));
  CHECK(ossia::convert<std::string>(rig.param("/F/Strobe/preset").value()) == "Open");
  CHECK(rig.at(0) == 255);
}

TEST_CASE("Percent default values scale to the channel resolution", "[dmx][fixture]")
{
  auto def = parse(R"_(
    "availableChannels": {
      "Speed": {"defaultValue": "100%", "capability": {"type": "PanTiltSpeed", "speedStart": "fast", "speedEnd": "slow"}},
      "Half": {"fineChannelAliases": ["Half fine"], "defaultValue": "50%", "capability": {"type": "Intensity"}}
    },
    "modes": [{"name": "m", "channels": ["Speed", "Half", "Half fine"]}]
  )_");
  Rig rig;
  rig.add(onlyMode(def));
  CHECK(rig.at(0) == 255);
  CHECK(rig.at(1) == 0x7F);
  CHECK(rig.at(2) == 0xFF);
}

TEST_CASE("Presets are named from the capability properties", "[dmx][fixture]")
{
  auto def = parse(R"_(
    "availableChannels": {
      "Shutter": {
        "capabilities": [
          {"dmxRange": [0, 9], "type": "NoFunction"},
          {"dmxRange": [10, 19], "type": "ShutterStrobe", "shutterEffect": "Closed"},
          {"dmxRange": [20, 255], "type": "ShutterStrobe", "shutterEffect": "Strobe", "speedStart": "1Hz", "speedEnd": "20Hz", "randomTiming": true}
        ]
      }
    },
    "modes": [{"name": "m", "channels": ["Shutter"]}]
  )_");
  const auto& chan = onlyMode(def).channels.at(0);
  const auto& caps = ossia::get<std::vector<Artnet::RangeCapability>>(chan.capabilities);
  REQUIRE(caps.size() == 3);
  CHECK(caps[0].effectName == "No function");
  CHECK(caps[1].effectName == "Closed");
  CHECK(caps[2].effectName == "Strobe 1Hz to 20Hz random");

  Rig rig;
  rig.add(onlyMode(def));
  rig.param("/F/Shutter/preset").push_value(std::string("No function"));
  CHECK(rig.at(0) == 0);
  rig.param("/F/Shutter/preset").push_value(std::string("Closed"));
  CHECK(rig.at(0) == 10);
}

TEST_CASE("Wheel slot ranges are named after their slots", "[dmx][fixture]")
{
  auto def = parse(R"_(
    "wheels": {"Color Wheel": {"slots": [{"type": "Open"}, {"type": "Color", "name": "Red"}, {"type": "Color", "name": "Blue"}]}},
    "availableChannels": {
      "Color Wheel": {
        "capabilities": [
          {"dmxRange": [0, 9], "type": "WheelSlot", "slotNumber": 1},
          {"dmxRange": [10, 19], "type": "WheelSlot", "slotNumber": 2},
          {"dmxRange": [20, 29], "type": "WheelShake", "slotNumberStart": 2, "slotNumberEnd": 3}
        ]
      }
    },
    "modes": [{"name": "m", "channels": ["Color Wheel"]}]
  )_");
  const auto& caps = ossia::get<std::vector<Artnet::RangeCapability>>(
      onlyMode(def).channels.at(0).capabilities);
  REQUIRE(caps.size() == 3);
  CHECK(caps[0].effectName == "Open");
  CHECK(caps[1].effectName == "Red");
  CHECK(caps[2].effectName == "WheelShake Red to Blue");
}

TEST_CASE("A constant channel holds its value and exposes no control", "[dmx][fixture]")
{
  auto def = parse(R"_(
    "availableChannels": {
      "Mode": {"constant": true, "defaultValue": 42, "capability": {"type": "Maintenance"}},
      "Dimmer": {"capability": {"type": "Intensity"}}
    },
    "modes": [{"name": "m", "channels": ["Mode", "Dimmer"]}]
  )_");
  Rig rig;
  rig.add(onlyMode(def));
  CHECK(rig.node("/F/Mode") == nullptr);
  CHECK(rig.node("/F/Dimmer") != nullptr);
  CHECK(rig.at(0) == 42);
}

TEST_CASE("A fixture is placed in its universe", "[dmx][fixture]")
{
  auto def = parse(R"_(
    "availableChannels": {"Dimmer": {"capability": {"type": "Intensity"}}},
    "modes": [{"name": "m", "channels": ["Dimmer"]}]
  )_");
  Rig rig;
  rig.add(onlyMode(def), 10, 3, 2);

  REQUIRE(rig.buffer.universes() >= 2);
  rig.buffer.dirty.reset();
  rig.param("/F/Dimmer").push_value(200);
  CHECK(rig.at(512 + 10) == 200);
  CHECK(rig.at(10) == 0);
  CHECK(rig.buffer.dirty[1]);
  CHECK(!rig.buffer.dirty[0]);
}

TEST_CASE("Switching channels expose every channel they switch to", "[dmx][fixture]")
{
  auto def = parse(R"_(
    "availableChannels": {
      "Function": {
        "defaultValue": 200,
        "capabilities": [
          {"dmxRange": [0, 127], "type": "Effect", "effectName": "Auto", "switchChannels": {"Speed or Preset": "Speed"}},
          {"dmxRange": [128, 255], "type": "Effect", "effectName": "Static", "switchChannels": {"Speed or Preset": "Preset"}}
        ]
      },
      "Speed": {"capability": {"type": "EffectSpeed", "speedStart": "slow", "speedEnd": "fast"}},
      "Preset": {
        "capabilities": [
          {"dmxRange": [0, 99], "type": "ColorPreset", "comment": "Red"},
          {"dmxRange": [100, 255], "type": "ColorPreset", "comment": "Blue"}
        ]
      }
    },
    "modes": [{"name": "m", "channels": ["Function", "Speed or Preset"]}]
  )_");
  const auto& mode = onlyMode(def);
  REQUIRE(mode.allChannels.size() == 2);
  REQUIRE(mode.channels.size() == 2);

  const auto& sw = mode.channels[1];
  CHECK(sw.name == "Speed or Preset");
  const auto& caps = ossia::get<std::vector<Artnet::RangeCapability>>(sw.capabilities);
  REQUIRE(caps.size() == 3);
  CHECK(caps[0].effectName == "Speed");
  CHECK(caps[1].effectName == "Preset: Red");
  CHECK(caps[2].effectName == "Preset: Blue");

  Rig rig;
  rig.add(mode);
  rig.param("/F/Speed or Preset/preset").push_value(std::string("Preset: Blue"));
  CHECK(rig.at(1) == 100);
}

TEST_CASE("Switching channels work inside matrix templates", "[dmx][fixture][matrix]")
{
  auto def = parse(R"_(
    "matrix": {"pixelKeys": [[["1", "2"]]]},
    "templateChannels": {
      "FX $pixelKey": {
        "capabilities": [
          {"dmxRange": [0, 127], "type": "Effect", "effectName": "Flash", "switchChannels": {"Rate or Speed $pixelKey": "Rate $pixelKey"}},
          {"dmxRange": [128, 255], "type": "Effect", "effectName": "Chase", "switchChannels": {"Rate or Speed $pixelKey": "Speed $pixelKey"}}
        ]
      },
      "Rate $pixelKey": {"capability": {"type": "ShutterStrobe", "shutterEffect": "Strobe"}},
      "Speed $pixelKey": {"capability": {"type": "EffectSpeed"}}
    },
    "modes": [{
      "name": "m",
      "channels": [
        {"insert": "matrixChannels", "repeatFor": "eachPixelABC", "channelOrder": "perPixel",
         "templateChannels": ["FX $pixelKey", "Rate or Speed $pixelKey"]},
        "Rate or Speed 1"
      ]
    }]
  )_");
  const auto& mode = onlyMode(def);
  const std::vector<QString> expected{
      "FX 1", "Rate or Speed 1", "FX 2", "Rate or Speed 2", "Rate or Speed 1"};
  CHECK(mode.allChannels == expected);
  REQUIRE(mode.channels.size() == 5);

  const auto& caps
      = ossia::get<std::vector<Artnet::RangeCapability>>(mode.channels[3].capabilities);
  REQUIRE(caps.size() == 2);
  CHECK(caps[0].effectName == "Rate 2");
  CHECK(caps[1].effectName == "Speed 2");
}

TEST_CASE("The fine channel of a switched channel is driven", "[dmx][fixture]")
{
  auto def = parse(R"_(
    "availableChannels": {
      "Program": {
        "capabilities": [
          {"dmxRange": [0, 127], "type": "Effect", "effectName": "Manual", "switchChannels": {"Red or Step": "Red"}},
          {"dmxRange": [128, 255], "type": "Effect", "effectName": "Program", "switchChannels": {"Red or Step": "Step"}}
        ]
      },
      "Red": {"fineChannelAliases": ["Red fine"], "capability": {"type": "ColorIntensity", "color": "Red"}},
      "Step": {"capability": {"type": "EffectDuration"}}
    },
    "modes": [{"name": "m", "channels": ["Program", "Red or Step", "Red fine"]}]
  )_");
  Rig rig;
  rig.add(onlyMode(def));
  CHECK(rig.max("/F/Red or Step") == 65535);
  rig.param("/F/Red or Step").push_value(0x1234);
  CHECK(rig.at(1) == 0x12);
  CHECK(rig.at(2) == 0x34);
}

TEST_CASE("A fine slot can itself be a switching channel", "[dmx][fixture]")
{
  auto def = parse(R"_(
    "availableChannels": {
      "Mode": {
        "capabilities": [
          {"dmxRange": [0, 127], "type": "Effect", "effectName": "Index",
           "switchChannels": {"Rotation": "Rotation Angle", "Rotation fine": "Rotation Angle fine"}},
          {"dmxRange": [128, 255], "type": "Effect", "effectName": "Spin",
           "switchChannels": {"Rotation": "Rotation Speed", "Rotation fine": "Rotation Speed fine"}}
        ]
      },
      "Rotation Angle": {"fineChannelAliases": ["Rotation Angle fine"], "capability": {"type": "WheelSlotRotation"}},
      "Rotation Speed": {"fineChannelAliases": ["Rotation Speed fine"], "capability": {"type": "WheelRotation"}}
    },
    "modes": [{"name": "m", "channels": ["Mode", "Rotation", "Rotation fine"]}]
  )_");
  Rig rig;
  rig.add(onlyMode(def));
  CHECK(rig.max("/F/Rotation") == 65535);
  rig.param("/F/Rotation").push_value(0xBEEF);
  CHECK(rig.at(1) == 0xBE);
  CHECK(rig.at(2) == 0xEF);
}

TEST_CASE(
    "Template names may hold the pixel key more than once", "[dmx][fixture][matrix]")
{
  auto def = parse(R"_(
    "matrix": {"pixelKeys": [[["1", "2"]]], "pixelGroups": {"All": "all"}},
    "templateChannels": {
      "Show $pixelKey": {
        "capabilities": [
          {"dmxRange": [0, 255], "type": "Effect", "effectName": "Show",
           "switchChannels": {"Speed $pixelKey / Sensitivity $pixelKey": "Speed $pixelKey"}}
        ]
      },
      "Speed $pixelKey": {"capability": {"type": "EffectSpeed"}}
    },
    "modes": [{"name": "m", "channels": ["Show All", "Speed All / Sensitivity All"]}]
  )_");
  const auto& mode = onlyMode(def);
  REQUIRE(mode.channels.size() == 2);
  CHECK(mode.channels[1].name == "Speed All / Sensitivity All");
}

TEST_CASE("Matrix modes keep every slot, including empty ones", "[dmx][fixture][matrix]")
{
  auto def = parse(R"_(
    "matrix": {"pixelCount": [2, 1, 1]},
    "templateChannels": {
      "Red $pixelKey": {"fineChannelAliases": ["Red $pixelKey fine"], "capability": {"type": "ColorIntensity", "color": "Red"}},
      "Blue $pixelKey": {"capability": {"type": "ColorIntensity", "color": "Blue"}}
    },
    "modes": [{
      "name": "m",
      "channels": [{"insert": "matrixChannels", "repeatFor": "eachPixelXYZ", "channelOrder": "perPixel",
                    "templateChannels": ["Red $pixelKey", "Red $pixelKey fine", null, "Blue $pixelKey"]}]
    }]
  )_");
  const auto& mode = onlyMode(def);
  const std::vector<QString> expected{"Red 1", "Red 1 fine", "", "Blue 1",
                                      "Red 2", "Red 2 fine", "", "Blue 2"};
  CHECK(mode.allChannels == expected);

  Rig rig;
  rig.add(mode);
  CHECK(rig.max("/F/Red 2") == 65535);
  rig.param("/F/Red 2").push_value(0x0102);
  rig.param("/F/Blue 2").push_value(7);
  CHECK(rig.at(4) == 0x01);
  CHECK(rig.at(5) == 0x02);
  CHECK(rig.at(7) == 7);
}

TEST_CASE("Matrix channels can be ordered per channel", "[dmx][fixture][matrix]")
{
  auto def = parse(R"_(
    "matrix": {"pixelKeys": [[["A", "B"]]]},
    "templateChannels": {
      "Red $pixelKey": {"capability": {"type": "ColorIntensity", "color": "Red"}},
      "Green $pixelKey": {"capability": {"type": "ColorIntensity", "color": "Green"}}
    },
    "modes": [{
      "name": "m",
      "channels": [{"insert": "matrixChannels", "repeatFor": ["A", "B"], "channelOrder": "perChannel",
                    "templateChannels": ["Red $pixelKey", "Green $pixelKey"]}]
    }]
  )_");
  const std::vector<QString> expected{"Red A", "Red B", "Green A", "Green B"};
  CHECK(onlyMode(def).allChannels == expected);
}

TEST_CASE("eachPixelABC sorts pixel keys alphanumerically", "[dmx][fixture][matrix]")
{
  auto def = parse(R"_(
    "matrix": {"pixelKeys": [[["10", "2", "1"]]]},
    "templateChannels": {"Dimmer $pixelKey": {"capability": {"type": "Intensity"}}},
    "modes": [{
      "name": "m",
      "channels": [{"insert": "matrixChannels", "repeatFor": "eachPixelABC", "channelOrder": "perPixel",
                    "templateChannels": ["Dimmer $pixelKey"]}]
    }]
  )_");
  const std::vector<QString> expected{"Dimmer 1", "Dimmer 2", "Dimmer 10"};
  CHECK(onlyMode(def).allChannels == expected);
}

TEST_CASE(
    "Two-dimensional pixel counts use the library's pixel keys",
    "[dmx][fixture][matrix]")
{
  auto def = parse(R"_(
    "matrix": {"pixelCount": [2, 2, 1]},
    "templateChannels": {"Dimmer $pixelKey": {"capability": {"type": "Intensity"}}},
    "modes": [{
      "name": "m",
      "channels": [{"insert": "matrixChannels", "repeatFor": "eachPixelXYZ", "channelOrder": "perPixel",
                    "templateChannels": ["Dimmer $pixelKey"]}]
    }]
  )_");
  const std::vector<QString> expected{
      "Dimmer (1, 1)", "Dimmer (2, 1)", "Dimmer (1, 2)", "Dimmer (2, 2)"};
  CHECK(onlyMode(def).allChannels == expected);
}

TEST_CASE(
    "Modes can name template channels per pixel and per group", "[dmx][fixture][matrix]")
{
  auto def = parse(R"_(
    "matrix": {
      "pixelKeys": [[["1", "2"]]],
      "pixelGroups": {"All": "all"}
    },
    "templateChannels": {
      "Red $pixelKey": {"capability": {"type": "ColorIntensity", "color": "Red"}}
    },
    "modes": [{"name": "m", "channels": ["Red All", "Red 1", "Red 2", "Red 3"]}]
  )_");
  const auto& mode = onlyMode(def);
  REQUIRE(mode.allChannels.size() == 4);
  REQUIRE(mode.channels.size() == 3);
  CHECK(mode.channels[0].name == "Red All");
  CHECK(mode.channels[1].name == "Red 1");
  CHECK(mode.channels[2].name == "Red 2");

  Rig rig;
  rig.add(mode);
  rig.param("/F/Red 2").push_value(9);
  CHECK(rig.at(2) == 9);
}

TEST_CASE("Redirects and malformed documents are rejected or skipped", "[dmx][fixture]")
{
  CHECK(!OFL::parseFixture(
      R"_({"$schema":"https://x/schemas/fixture-redirect.json","name":"Old","redirectTo":"a/b","reason":"FixtureRenamed"})_"));
  CHECK(!OFL::parseFixture("{not json"));
  CHECK(!OFL::parseFixture(R"_({"name": 3})_"));

  auto def = parse(R"_(
    "categories": ["Dimmer", 3],
    "availableChannels": {
      "A": {"capability": {"type": 1, "comment": [], "effectName": {}}},
      "B": {"capabilities": "nope"},
      "C": {"capabilities": [{"type": "Effect"}, {"dmxRange": [0], "type": "Effect"}, {"dmxRange": [0, 255]}, 4]},
      "D": {"defaultValue": "lots", "fineChannelAliases": [1]},
      "E": 5
    },
    "modes": [{"name": "m", "channels": ["A", "B", "C", "D", "E", 7, null, {"insert": "other"}]}, {"name": "no channels"}]
  )_");
  CHECK(def.categories == QStringList{"Dimmer"});
  const auto& mode = onlyMode(def);
  CHECK(mode.allChannels.size() == 8);
  CHECK(mode.channels.size() == 5);

  Rig rig;
  rig.add(mode);
}

TEST_CASE("Channel settings survive serialization", "[dmx][fixture][serialization]")
{
  Artnet::Channel chan;
  chan.name = "Gobo";
  chan.defaultValue = 300;
  chan.resolution = 2;
  chan.constant = true;
  chan.fineChannels = {"Gobo fine"};
  chan.capabilities = std::vector<Artnet::RangeCapability>{
      Artnet::RangeCapability{{"WheelSlot", "a comment", "Red"}, {0, 1000}},
      Artnet::RangeCapability{{"WheelSlot", "", "Blue"}, {1001, 2000}, Artnet::MenuClick::Hidden}};

  auto check = [&](const Artnet::Channel& c) {
    CHECK(c.name == chan.name);
    CHECK(c.defaultValue == chan.defaultValue);
    CHECK(c.resolution == chan.resolution);
    CHECK(c.constant == chan.constant);
    CHECK(c.fineChannels == chan.fineChannels);
    const auto& caps = ossia::get<std::vector<Artnet::RangeCapability>>(c.capabilities);
    REQUIRE(caps.size() == 2);
    CHECK(caps[0].effectName == "Red");
    CHECK(caps[0].comment == "a comment");
    CHECK(caps[0].range == std::pair{0, 1000});
    CHECK(caps[0].menuClick == Artnet::MenuClick::Start);
    CHECK(caps[1].menuClick == Artnet::MenuClick::Hidden);
  };

  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    check(JSONWriter::unmarshall<Artnet::Channel>(
        readJson(JSONReader::marshall(chan).toByteArray())));
    check(
        DataStreamWriter::unmarshall<Artnet::Channel>(DataStreamReader::marshall(chan)));
  });
}

TEST_CASE("Channels from older binary saves still load", "[dmx][fixture][serialization]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    Artnet::Channel old;
    old.name = "Dimmer";
    old.defaultValue = 12;
    old.fineChannels = {"Dimmer fine"};
    old.capabilities = std::vector<Artnet::RangeCapability>{
        Artnet::RangeCapability{{"Intensity", "", "Full"}, {0, 255}}};

    QByteArray bytes;
    {
      DataStreamReader r{&bytes};
      r.m_stream << old.name << old.defaultValue << old.capabilities << old.fineChannels;
      r.insertDelimiter();
      r.m_stream << QString("next");
    }

    DataStreamWriter w{bytes};
    Artnet::Channel loaded;
    w.writeTo(loaded);
    QString next;
    w.m_stream >> next;
    CHECK(loaded.name == "Dimmer");
    CHECK(loaded.defaultValue == 12);
    CHECK(loaded.resolution == 0);
    CHECK(!loaded.constant);
    CHECK(loaded.fineChannels == old.fineChannels);
    CHECK(next == "next");
  });
}

TEST_CASE(
    "Channels saved without a resolution keep their meaning",
    "[dmx][fixture][serialization]")
{
  Artnet::Channel legacy;
  legacy.defaultValue = 0;
  legacy.capabilities = std::vector<Artnet::RangeCapability>{
      Artnet::RangeCapability{{"Effect", "", "A"}, {0, 127}},
      Artnet::RangeCapability{{"Effect", "", "B"}, {128, 255}}};
  CHECK(legacy.valueResolution() == 1);

  legacy.capabilities = std::vector<Artnet::RangeCapability>{
      Artnet::RangeCapability{{"Effect", "", "A"}, {0, 32767}},
      Artnet::RangeCapability{{"Effect", "", "B"}, {32768, 65535}}};
  CHECK(legacy.valueResolution() == 2);
}

namespace
{
Artnet::Fixture ledStrip(int pixels, int diodes, bool reverse = false)
{
  Artnet::Fixture f;
  f.fixtureName = "L";
  f.led = Artnet::LEDStripLayout{
      .diodes = std::vector<Artnet::Diode>(diodes, Artnet::Diode::R),
      .length = pixels,
      .reverse = reverse};
  return f;
}

std::vector<ossia::value> ramp(int n)
{
  std::vector<ossia::value> v;
  for(int i = 0; i < n; i++)
    v.push_back(i + 1);
  return v;
}
}

TEST_CASE("Writes ending on a universe boundary stay in that universe", "[dmx][buffer]")
{
  ossia::net::dmx_buffer buffer{512};
  buffer.set_universe_count(2);

  CHECK(buffer.write_channels(0, 512).size() == 512);
  CHECK(buffer.dirty[0]);
  CHECK(!buffer.dirty[1]);

  buffer.dirty.reset();
  CHECK(buffer.write_channels(510, 4).size() == 4);
  CHECK(buffer.dirty[0]);
  CHECK(buffer.dirty[1]);

  CHECK(buffer.write_channels(1000, 25).empty());
  CHECK(buffer.write_channels(0, 0).empty());
}

TEST_CASE("An LED strip filling a whole universe is written", "[dmx][led]")
{
  Rig rig;
  addArtnetFixture(rig.dev, rig.buffer, ledStrip(128, 4), 0, 512);
  CHECK(rig.buffer.universes() == 1);

  auto values = ramp(512);
  values[0] = 300;
  values[1] = -5;
  rig.param("/L").push_value(values);
  CHECK(rig.at(0) == 255);
  CHECK(rig.at(1) == 0);
  CHECK(rig.at(2) == 3);
  CHECK(rig.at(511) == 255);
  CHECK(rig.at(510) == 255);
  CHECK(rig.buffer.dirty[0]);
}

TEST_CASE("A reversed LED strip writes its pixels from the end", "[dmx][led]")
{
  Rig rig;
  addArtnetFixture(rig.dev, rig.buffer, ledStrip(3, 3, true), 0, 512);
  rig.param("/L").push_value(ramp(9));
  const int expected[] = {7, 8, 9, 4, 5, 6, 1, 2, 3};
  for(int i = 0; i < 9; i++)
    CHECK(rig.at(i) == expected[i]);
}

TEST_CASE("Addresses roll over to the next universe at its end", "[dmx][address]")
{
  Artnet::Fixture dimmer;
  dimmer.mode.channelNames = std::vector<QString>(4, "D");

  Artnet::Fixture f = dimmer;
  f.address = 512;
  Artnet::normalizeAddress(f, 512);
  CHECK(f.universe == 1);
  CHECK(f.address == 0);

  f.address = 511;
  f.universe = 0;
  Artnet::normalizeAddress(f, 512);
  CHECK(f.universe == 0);
  CHECK(f.address == 511);

  std::vector<Artnet::Fixture> fixtures{ledStrip(128, 4)};
  CHECK(Artnet::nextAvailableAddress(fixtures, 512) == std::pair{1, 0});

  fixtures = {dimmer, ledStrip(170, 3)};
  fixtures[0].address = 300;
  CHECK(Artnet::nextAvailableAddress(fixtures, 512) == std::pair{0, 510});

  fixtures[1].universe = 2;
  fixtures[1].address = 10;
  CHECK(Artnet::nextAvailableAddress(fixtures, 100) == std::pair{7, 20});
  CHECK(Artnet::nextAvailableAddress({}, 512) == std::pair{0, 0});
}

TEST_CASE("Repeated fixtures are separated by their spacing", "[dmx][address]")
{
  auto strips = Artnet::repeatFixture(ledStrip(10, 3), 5, 3, 2, 512);
  REQUIRE(strips.size() == 3);
  CHECK(strips[0].address == 5);
  CHECK(strips[1].address == 37);
  CHECK(strips[2].address == 69);
  CHECK(Artnet::repeatFixture(ledStrip(10, 3), 0, 0, 0, 512).empty());
}

TEST_CASE("Repeated fixtures never straddle two universes", "[dmx][address]")
{
  Artnet::Fixture big;
  big.mode.channelNames = std::vector<QString>(200, "D");
  big.universe = 3;
  auto f = Artnet::repeatFixture(big, 0, 3, 0, 512);
  REQUIRE(f.size() == 3);
  CHECK(std::pair{f[0].universe, f[0].address} == std::pair{3, 0});
  CHECK(std::pair{f[1].universe, f[1].address} == std::pair{3, 200});
  CHECK(std::pair{f[2].universe, f[2].address} == std::pair{4, 0});

  auto strips = Artnet::repeatFixture(ledStrip(170, 3), 0, 2, 0, 512);
  CHECK(std::pair{strips[1].universe, strips[1].address} == std::pair{1, 0});

  auto first = Artnet::repeatFixture(ledStrip(10, 3), 700, 1, 0, 512);
  CHECK(std::pair{first[0].universe, first[0].address} == std::pair{1, 188});

  CHECK(Artnet::fitInUniverse(0, 500, 20, 512) == std::pair{1, 0});
  CHECK(Artnet::fitInUniverse(0, 492, 20, 512) == std::pair{0, 492});
  CHECK(Artnet::fitInUniverse(0, 500, 1000, 512) == std::pair{0, 500});
  CHECK(Artnet::fitInUniverse(2, 1100, 4, 512) == std::pair{4, 76});
}

TEST_CASE("Huge repeats do not overflow", "[dmx][address]")
{
  auto strips = Artnet::repeatFixture(
      ledStrip(1'000'000, 6), 512 * 65539, 512, 0, 512);
  REQUIRE(strips.size() == 512);
  CHECK(strips.back().universe > strips.front().universe);
  CHECK(strips.back().address >= 0);

  Artnet::Fixture box;
  box.fixtureName = "B";
  box.led = Artnet::LEDVolumeLayout{
      .diodes = std::vector<Artnet::Diode>(6, Artnet::Diode::R),
      .width = 1024, .height = 1024, .depth = 1024};
  CHECK(Artnet::usedChannels(box) == std::numeric_limits<int>::max());

  Rig rig;
  addArtnetFixture(rig.dev, rig.buffer, box, 0, 512);
  CHECK(rig.node("/B") == nullptr);
  CHECK(rig.buffer.universes() == 1);
}

TEST_CASE("The buffer is sized for every fixture up front", "[dmx][address]")
{
  std::vector<Artnet::Fixture> fixtures{ledStrip(170, 3), ledStrip(10, 3)};
  fixtures[1].universe = 4;
  fixtures[1].address = 500;
  CHECK(requiredUniverses(fixtures, 1, 512) == 5);
  CHECK(requiredUniverses({}, 0, 512) == 1);
  fixtures[1].universe = 0;
  CHECK(requiredUniverses(fixtures, 1, 512) == 2);
}

TEST_CASE("A switched channel keeps the resolution of its target", "[dmx][fixture]")
{
  auto def = parse(R"_(
    "availableChannels": {
      "Mode": {
        "defaultValue": 200,
        "capabilities": [
          {"dmxRange": [0, 127], "type": "Effect", "effectName": "Index", "switchChannels": {"Gobo": "Gobo Index"}},
          {"dmxRange": [128, 255], "type": "Effect", "effectName": "Spin", "switchChannels": {"Gobo": "Gobo Spin"}}
        ]
      },
      "Gobo Index": {"capability": {"type": "WheelSlotRotation"}},
      "Gobo Spin": {
        "fineChannelAliases": ["Gobo Spin fine"],
        "defaultValue": 32896,
        "capabilities": [
          {"dmxRange": [0, 600], "type": "Effect", "effectName": "Off"},
          {"dmxRange": [601, 32130], "type": "Effect", "effectName": "CW"},
          {"dmxRange": [32131, 32895], "type": "Effect", "effectName": "Stop"},
          {"dmxRange": [32896, 65535], "type": "Effect", "effectName": "CCW"}
        ]
      }
    },
    "modes": [{"name": "m", "channels": ["Mode", "Gobo", "Gobo Spin fine"]}]
  )_");
  const auto& mode = onlyMode(def);
  REQUIRE(mode.channels.size() == 2);
  const auto& gobo = mode.channels[1];
  CHECK(gobo.resolution == 2);
  CHECK(gobo.defaultValue == 32896);
  const auto& caps = ossia::get<std::vector<Artnet::RangeCapability>>(gobo.capabilities);
  REQUIRE(caps.size() == 5);
  CHECK(caps[0].range == std::pair{0, 65535});
  CHECK(caps[2].range == std::pair{601, 32130});
  CHECK(caps[3].range == std::pair{32131, 32895});

  Rig rig;
  rig.add(mode);
  CHECK(rig.max("/F/Gobo") == 65535);
  CHECK(rig.at(1) == 0x80);
  CHECK(rig.at(2) == 0x80);
  rig.param("/F/Gobo/preset").push_value(std::string("Gobo Spin: Stop"));
  CHECK(rig.at(1) == (32131 >> 8));
  CHECK(rig.at(2) == (32131 & 0xFF));
}

TEST_CASE(
    "A fine byte reached through a switching alias drives its coarse channel",
    "[dmx][fixture]")
{
  auto def = parse(R"_(
    "availableChannels": {
      "Programs": {
        "capabilities": [
          {"dmxRange": [0, 9], "type": "NoFunction", "switchChannels": {"Dimmer fine / Step": "Dimmer fine"}},
          {"dmxRange": [10, 255], "type": "Effect", "effectName": "Program", "switchChannels": {"Dimmer fine / Step": "Step"}}
        ]
      },
      "Dimmer": {"fineChannelAliases": ["Dimmer fine"], "capability": {"type": "Intensity"}},
      "Step": {"capability": {"type": "EffectDuration"}}
    },
    "modes": [{"name": "m", "channels": ["Programs", "Dimmer", "Dimmer fine / Step"]}]
  )_");
  Rig rig;
  rig.add(onlyMode(def));
  CHECK(rig.node("/F/Dimmer fine / Step") == nullptr);
  CHECK(rig.max("/F/Dimmer") == 65535);
  rig.param("/F/Dimmer").push_value(0x1234);
  CHECK(rig.at(1) == 0x12);
  CHECK(rig.at(2) == 0x34);
}

TEST_CASE("A switching alias can target a matrix channel", "[dmx][fixture][matrix]")
{
  auto def = parse(R"_(
    "matrix": {"pixelKeys": [[["1", "2"]]]},
    "availableChannels": {
      "Program": {
        "capabilities": [
          {"dmxRange": [0, 9], "type": "NoFunction", "switchChannels": {"Red 1 / Speed": "Red 1"}},
          {"dmxRange": [10, 255], "type": "Effect", "effectName": "Run", "switchChannels": {"Red 1 / Speed": "Speed"}}
        ]
      },
      "Speed": {"capability": {"type": "EffectSpeed"}}
    },
    "templateChannels": {
      "Red $pixelKey": {
        "capabilities": [
          {"dmxRange": [0, 127], "type": "ColorIntensity", "color": "Red", "effectName": "Low"},
          {"dmxRange": [128, 255], "type": "ColorIntensity", "color": "Red", "effectName": "High"}
        ]
      }
    },
    "modes": [{"name": "m", "channels": ["Program", "Red 1 / Speed", "Red 2"]}]
  )_");
  const auto& mode = onlyMode(def);
  REQUIRE(mode.channels.size() == 3);
  const auto& caps
      = ossia::get<std::vector<Artnet::RangeCapability>>(mode.channels[1].capabilities);
  REQUIRE(caps.size() == 3);
  CHECK(caps[0].effectName == "Red 1: Low");
  CHECK(caps[1].effectName == "Red 1: High");
  CHECK(caps[2].effectName == "Speed");
}

TEST_CASE(
    "An available channel overrides the matrix channel of the same name",
    "[dmx][fixture][matrix]")
{
  auto def = parse(R"_(
    "matrix": {"pixelKeys": [[["1", "2"]]]},
    "availableChannels": {
      "Speed 1": {"capabilities": [{"dmxRange": [0, 255], "type": "Effect", "effectName": "Own"}]}
    },
    "templateChannels": {
      "Speed $pixelKey": {"capabilities": [{"dmxRange": [0, 255], "type": "Effect", "effectName": "Generic"}]}
    },
    "modes": [{"name": "m", "channels": [
      {"insert": "matrixChannels", "repeatFor": "eachPixelABC", "channelOrder": "perPixel", "templateChannels": ["Speed $pixelKey"]}
    ]}]
  )_");
  const auto& mode = onlyMode(def);
  REQUIRE(mode.channels.size() == 2);
  auto effect = [&](int i) {
    return ossia::get<std::vector<Artnet::RangeCapability>>(mode.channels[i].capabilities)
        .front()
        .effectName;
  };
  CHECK(effect(0) == "Own");
  CHECK(effect(1) == "Generic");
}

TEST_CASE("Presets set the value their menuClick asks for", "[dmx][fixture]")
{
  auto def = parse(R"_(
    "availableChannels": {
      "Strobe": {
        "capabilities": [
          {"dmxRange": [0, 10], "type": "ShutterStrobe", "shutterEffect": "Closed"},
          {"dmxRange": [11, 250], "type": "ShutterStrobe", "shutterEffect": "Strobe", "menuClick": "center"},
          {"dmxRange": [251, 254], "type": "Effect", "effectName": "Last", "menuClick": "end"},
          {"dmxRange": [255, 255], "type": "ShutterStrobe", "shutterEffect": "Open", "menuClick": "hidden"}
        ]
      }
    },
    "modes": [{"name": "m", "channels": ["Strobe"]}]
  )_");
  const auto& caps = ossia::get<std::vector<Artnet::RangeCapability>>(
      onlyMode(def).channels[0].capabilities);
  REQUIRE(caps.size() == 4);
  CHECK(caps[0].menuClick == Artnet::MenuClick::Start);
  CHECK(caps[1].menuClick == Artnet::MenuClick::Center);
  CHECK(caps[2].menuClick == Artnet::MenuClick::End);
  CHECK(caps[3].menuClick == Artnet::MenuClick::Hidden);

  Rig rig;
  rig.add(onlyMode(def));
  auto& preset = rig.param("/F/Strobe/preset");
  const auto* names = preset.get_domain().v.target<ossia::domain_base<std::string>>();
  REQUIRE(names);
  CHECK(names->values.size() == 3);

  preset.push_value(std::string("Strobe"));
  CHECK(rig.at(0) == 130);
  preset.push_value(std::string("Last"));
  CHECK(rig.at(0) == 254);
}

TEST_CASE("A preset value is scaled up into a 16-bit channel by OFL rules", "[dmx][fixture]")
{
  CHECK(OFL::menuClickValue(Artnet::RangeCapability{{}, {10, 20}, Artnet::MenuClick::End}, 1, 2) == 0x14FF);
  CHECK(OFL::menuClickValue(Artnet::RangeCapability{{}, {10, 20}, Artnet::MenuClick::Center}, 1, 2) == (0x0A00 + 0x14FF) / 2);
}

TEST_CASE("Values are scaled between resolutions as OFL does", "[dmx][fixture]")
{
  CHECK(OFL::scaleValue(128, 1, 2) == 0x8080);
  CHECK(OFL::scaleValue(0xFF, 1, 3) == 0xFFFFFF);
  CHECK(OFL::scaleValue(0x1234, 2, 1) == 0x12);
  CHECK(OFL::scaleValue(300, 1, 1) == 255);

  CHECK(OFL::scaleRange(10, 20, 1, 2) == std::pair<int64_t, int64_t>{0x0A00, 0x14FF});
  CHECK(OFL::scaleRange(55809, 65535, 2, 1) == std::pair<int64_t, int64_t>{219, 255});
  CHECK(OFL::scaleRange(0x1200, 0x12FF, 2, 1) == std::pair<int64_t, int64_t>{0x12, 0x12});
  CHECK(OFL::scaleRange(0x1201, 0x12FF, 2, 1) == std::pair<int64_t, int64_t>{0x12, 0x12});
  CHECK(OFL::scaleRange(1, 300, 2, 1) == std::pair<int64_t, int64_t>{1, 1});
}

TEST_CASE("Range starts round up when a channel loses its fine byte", "[dmx][fixture]")
{
  auto def = parse(R"_(
    "availableChannels": {
      "Hue": {
        "fineChannelAliases": ["Hue fine"],
        "capabilities": [
          {"dmxRange": [0, 55808], "type": "Effect", "effectName": "Colors"},
          {"dmxRange": [55809, 65535], "type": "Effect", "effectName": "Rainbow"}
        ]
      }
    },
    "modes": [{"name": "8bit", "channels": ["Hue"]}]
  )_");
  Rig rig;
  rig.add(onlyMode(def));
  rig.param("/F/Hue/preset").push_value(std::string("Rainbow"));
  CHECK(rig.at(0) == 219);
}

TEST_CASE("Default values follow OFL scaling and percent rules", "[dmx][fixture]")
{
  auto def = parse(R"_(
    "availableChannels": {
      "Pan": {"fineChannelAliases": ["Pan fine"], "dmxValueResolution": "8bit", "defaultValue": 128,
              "capability": {"type": "Pan"}},
      "Dim": {"defaultValue": "50%", "capability": {"type": "Intensity"}}
    },
    "modes": [{"name": "m", "channels": ["Pan", "Pan fine", "Dim"]}]
  )_");
  const auto& mode = onlyMode(def);
  CHECK(mode.channels[1].defaultValue == 127);
  Rig rig;
  rig.add(mode);
  CHECK(rig.at(0) == 0x80);
  CHECK(rig.at(1) == 0x80);
  CHECK(rig.at(2) == 127);
}

TEST_CASE("Redirects to another brand load their target", "[dmx][fixture]")
{
  const std::string target = fixture(R"_(
    "categories": ["Color Changer"],
    "availableChannels": {"Dimmer": {"capability": {"type": "Intensity"}}},
    "modes": [{"name": "m", "channels": ["Dimmer"]}]
  )_");
  auto read = [&](const QString& key) -> std::optional<std::string> {
    if(key == "brand/original")
      return target;
    return std::nullopt;
  };
  auto redirect = [](std::string_view to, std::string_view reason) {
    return fmt::format(
        R"_({{"$schema": "https://raw.githubusercontent.com/OpenLightingProject/open-fixture-library/master/schemas/fixture-redirect.json", "name": "Rebadged", "redirectTo": "{}", "reason": "{}"}})_",
        to, reason);
  };

  auto def = OFL::parseFixture(redirect("brand/original", "SameAsDifferentBrand"), read);
  REQUIRE(def);
  CHECK(def->name == "Rebadged");
  CHECK(def->categories == QStringList{"Color Changer"});
  REQUIRE(def->modes.size() == 1);
  CHECK(def->modes[0].channels.size() == 1);

  CHECK(!OFL::parseFixture(redirect("brand/original", "FixtureRenamed"), read));
  CHECK(!OFL::parseFixture(redirect("brand/missing", "SameAsDifferentBrand"), read));
  CHECK(!OFL::parseFixture(redirect("brand/original", "SameAsDifferentBrand")));
  CHECK(OFL::parseFixture(target, read)->name == "Test");
}

TEST_CASE(
    "Channels saved before fine bytes were driven keep their 8-bit layout",
    "[dmx][fixture][serialization]")
{
  Artnet::Fixture f;
  f.fixtureName = "F";
  f.mode.channelNames = {"Gobo", "Gobo fine"};
  Artnet::Channel gobo;
  gobo.name = "Gobo";
  gobo.fineChannels = {"Gobo fine"};
  gobo.capabilities = std::vector<Artnet::RangeCapability>{
      Artnet::RangeCapability{{"WheelSlot", "", "Open"}, {0, 127}},
      Artnet::RangeCapability{{"WheelSlot", "", "Star"}, {128, 255}}};
  f.controls = {gobo};

  Rig rig;
  addArtnetFixture(rig.dev, rig.buffer, f, 0, 512);
  CHECK(rig.max("/F/Gobo") == 255);
  rig.param("/F/Gobo/preset").push_value(std::string("Star"));
  CHECK(rig.at(0) == 128);
  CHECK(rig.at(1) == 0);

  f.controls[0].resolution = 2;
  f.controls[0].capabilities = std::vector<Artnet::RangeCapability>{
      Artnet::RangeCapability{{"WheelSlot", "", "Open"}, {0, 32767}},
      Artnet::RangeCapability{{"WheelSlot", "", "Star"}, {32768, 65535}}};
  f.fixtureName = "G";
  addArtnetFixture(rig.dev, rig.buffer, f, 0, 512);
  CHECK(rig.max("/G/Gobo") == 65535);
}
