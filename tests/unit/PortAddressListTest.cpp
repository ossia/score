// Unit test: the addresses a port's address combo box lists
// (Process::listPortAddresses).
//
// Two rules are under test:
//  * a device is only walked when its capabilities declare it carries nodes of
//    the port's kind and direction (Device::NodeKind) - an OSC tree is never
//    visited for textures, a libav device is visited for video and sound;
//  * within a walked device, which nodes qualify: audio / texture / geometry
//    nodes by their parameter type (and the audio device's hardware ports by
//    their direction), MIDI streams as the device or one of its channels.

#include <State/Address.hpp>

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <Process/Dataflow/PortAddressComboBox.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/audio/audio_protocol.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/local/local.hpp>

#include <QString>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

using Process::PortType;
using Device::NodeKind;

namespace
{
// parameter_type is not settable from the outside; the real texture / geometry
// parameters live in plug-ins.
template <ossia::parameter_type T>
struct typed_parameter final : ossia::net::generic_parameter
{
  explicit typed_parameter(ossia::net::node_base& node)
      : ossia::net::generic_parameter{node}
  {
    m_type = T;
  }
};

Device::DeviceSettings settingsNamed(const QString& name)
{
  Device::DeviceSettings s;
  s.name = name;
  return s;
}

// A device that counts how often it is asked for its tree.
struct TestDevice final : Device::DeviceInterface
{
  ossia::net::generic_device dev;
  mutable int walked{};

  TestDevice(
      const QString& name, NodeKind kinds,
      std::unique_ptr<ossia::net::protocol_base> proto
      = std::make_unique<ossia::net::multiplex_protocol>())
      : Device::DeviceInterface{settingsNamed(name)}
      , dev{std::move(proto), name.toStdString()}
  {
    m_capas.nodeKinds = kinds;
  }

  bool reconnect() override { return true; }
  ossia::net::device_base* getDevice() const override
  {
    walked++;
    return const_cast<ossia::net::generic_device*>(&dev);
  }

  ossia::net::node_base& node(const std::string& path)
  {
    return ossia::net::find_or_create_node(dev.get_root_node(), path);
  }

  template <ossia::parameter_type T>
  void add(const std::string& path)
  {
    auto& n = node(path);
    n.set_parameter(std::make_unique<typed_parameter<T>>(n));
  }

  void addHardwareAudio(const std::string& path, bool output)
  {
    auto& n = node(path);
    n.set_parameter(
        std::make_unique<ossia::mapped_audio_parameter>(output, ossia::audio_mapping{}, n));
  }

  void addVirtualAudio(const std::string& path)
  {
    auto& n = node(path);
    n.set_parameter(std::make_unique<ossia::virtual_audio_parameter>(2, n));
  }
};

std::vector<QString> strings(const std::vector<State::Address>& addrs)
{
  std::vector<QString> res;
  for(const auto& a : addrs)
    res.push_back(a.toString());
  return res;
}

using L = std::vector<QString>;
}

TEST_CASE("A port kind maps to the device node kind of its direction", "[dataflow][port][address]")
{
  CHECK(Process::nodeKindOf(PortType::Message, true) == NodeKind::Value);
  CHECK(Process::nodeKindOf(PortType::Message, false) == NodeKind::Value);
  CHECK(Process::nodeKindOf(PortType::Audio, true) == NodeKind::AudioIn);
  CHECK(Process::nodeKindOf(PortType::Audio, false) == NodeKind::AudioOut);
  CHECK(Process::nodeKindOf(PortType::Midi, true) == NodeKind::MidiIn);
  CHECK(Process::nodeKindOf(PortType::Midi, false) == NodeKind::MidiOut);
  CHECK(Process::nodeKindOf(PortType::Texture, true) == NodeKind::TextureIn);
  CHECK(Process::nodeKindOf(PortType::Texture, false) == NodeKind::TextureOut);
  CHECK(Process::nodeKindOf(PortType::Geometry, true) == NodeKind::GeometryIn);
  CHECK(Process::nodeKindOf(PortType::Geometry, false) == NodeKind::GeometryOut);

  CHECK(Device::carries(NodeKind::TextureIn | NodeKind::AudioIn, NodeKind::AudioIn));
  CHECK(!Device::carries(NodeKind::TextureIn | NodeKind::AudioIn, NodeKind::AudioOut));
  CHECK(!Device::carries(NodeKind::Value, NodeKind::TextureIn));
}

TEST_CASE("Only the devices declaring the kind are walked", "[dataflow][port][address]")
{
  // An OSC-like device: values only, whatever its tree holds.
  TestDevice osc{"osc", NodeKind::Value};
  osc.add<ossia::parameter_type::TEXTURE>("/tex");
  osc.add<ossia::parameter_type::MESSAGE>("/foo/bar");

  CHECK(Process::listPortAddresses(osc, PortType::Texture, true).empty());
  CHECK(Process::listPortAddresses(osc, PortType::Audio, true).empty());
  CHECK(Process::listPortAddresses(osc, PortType::Midi, false).empty());
  CHECK(osc.walked == 0);

  // Direction is part of the kind: a camera is not a screen.
  TestDevice cam{"cam", NodeKind::TextureIn};
  cam.add<ossia::parameter_type::TEXTURE>("/");
  CHECK(Process::listPortAddresses(cam, PortType::Texture, false).empty());
  CHECK(cam.walked == 0);
  CHECK(strings(Process::listPortAddresses(cam, PortType::Texture, true)) == L{"cam:/"});
  CHECK(cam.walked == 1);
}

TEST_CASE("Texture nodes are listed at any depth, root included", "[dataflow][port][address]")
{
  // A Kinect-like device: the root and the per-stream children are textures,
  // a value node alongside is not listed.
  TestDevice kinect{"kinect", NodeKind::TextureIn};
  kinect.add<ossia::parameter_type::TEXTURE>("/");
  kinect.add<ossia::parameter_type::TEXTURE>("/rgb");
  kinect.add<ossia::parameter_type::TEXTURE>("/depth");
  kinect.add<ossia::parameter_type::GEOMETRY>("/points");
  kinect.add<ossia::parameter_type::MESSAGE>("/fps");
  kinect.node("/group"); // a bare container

  CHECK(
      strings(Process::listPortAddresses(kinect, PortType::Texture, true))
      == L{"kinect:/", "kinect:/rgb", "kinect:/depth"});
  CHECK(Process::listPortAddresses(kinect, PortType::Geometry, true).empty());

  TestDevice scan{"scan", NodeKind::TextureIn | NodeKind::GeometryIn};
  scan.add<ossia::parameter_type::TEXTURE>("/rgb");
  scan.add<ossia::parameter_type::GEOMETRY>("/points");
  CHECK(strings(Process::listPortAddresses(scan, PortType::Geometry, true)) == L{"scan:/points"});
  CHECK(strings(Process::listPortAddresses(scan, PortType::Texture, true)) == L{"scan:/rgb"});
}

TEST_CASE("The audio device lists its hardware ports by direction and its virtual ports both ways", "[dataflow][port][address]")
{
  // The hardware ports register with the audio protocol
  TestDevice audio{
      "audio", NodeKind::AudioIn | NodeKind::AudioOut,
      std::make_unique<ossia::audio_protocol>()};
  // What libossia's audio_protocol::setup_tree makes: plain audio parameters
  // under /in and /out, whose direction is where they sit...
  audio.add<ossia::parameter_type::AUDIO>("/in/main");
  audio.add<ossia::parameter_type::AUDIO>("/in/1");
  audio.add<ossia::parameter_type::AUDIO>("/in/2");
  audio.add<ossia::parameter_type::AUDIO>("/out/main");
  audio.add<ossia::parameter_type::AUDIO>("/out/1");
  // ... the custom ports score adds, which know theirs...
  audio.addHardwareAudio("/in/mic", false);
  audio.addHardwareAudio("/out/monitor", true);
  audio.addHardwareAudio("/custom/rec", false);
  audio.addHardwareAudio("/custom/send", true);
  // ... and a virtual port, which goes both ways.
  audio.addVirtualAudio("/bus");

  CHECK(
      strings(Process::listPortAddresses(audio, PortType::Audio, true))
      == L{"audio:/in/main", "audio:/in/1", "audio:/in/2", "audio:/in/mic",
           "audio:/custom/rec", "audio:/bus"});
  CHECK(
      strings(Process::listPortAddresses(audio, PortType::Audio, false))
      == L{"audio:/out/main", "audio:/out/1", "audio:/out/monitor",
           "audio:/custom/send", "audio:/bus"});

  // Not textures, whatever the kinds say
  CHECK(Process::listPortAddresses(audio, PortType::Texture, true).empty());
}

TEST_CASE("A MIDI stream is the device or one of its channels", "[dataflow][port][address]")
{
  TestDevice midi{"midi_in", NodeKind::MidiIn};
  for(int i = 1; i <= 16; i++)
  {
    const auto chan = "/" + std::to_string(i);
    midi.add<ossia::parameter_type::MESSAGE>(chan);
    midi.add<ossia::parameter_type::MESSAGE>(chan + "/note/60");
    midi.add<ossia::parameter_type::MESSAGE>(chan + "/control/7");
  }

  auto in = strings(Process::listPortAddresses(midi, PortType::Midi, true));
  REQUIRE(in.size() == 17);
  CHECK(in.front() == "midi_in:/");
  CHECK(in[1] == "midi_in:/1");
  CHECK(in.back() == "midi_in:/16");
  for(const auto& a : in)
    CHECK(!a.contains("note"));

  // An input device feeds no outlet
  CHECK(Process::listPortAddresses(midi, PortType::Midi, false).empty());
}

TEST_CASE("A device carrying video and sound is listed for both", "[dataflow][port][address]")
{
  // A libav / gstreamer / NDI input: one texture stream, one audio stream
  TestDevice libav{"libav", NodeKind::TextureIn | NodeKind::AudioIn};
  libav.add<ossia::parameter_type::TEXTURE>("/video");
  libav.add<ossia::parameter_type::AUDIO>("/audio");

  CHECK(strings(Process::listPortAddresses(libav, PortType::Texture, true)) == L{"libav:/video"});
  CHECK(strings(Process::listPortAddresses(libav, PortType::Audio, true)) == L{"libav:/audio"});
  CHECK(Process::listPortAddresses(libav, PortType::Audio, false).empty());
  CHECK(Process::listPortAddresses(libav, PortType::Texture, false).empty());
}
