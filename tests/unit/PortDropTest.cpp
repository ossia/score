// Dragging a device from the explorer onto a port.
//
// The drop carries the tree node, and a device node holds DeviceSettings where
// a parameter holds AddressSettings. The handler only ever looked for the
// latter, so dropping a device -- the only thing that makes sense for a port
// addressed by device, and the gesture the device explorer offers -- did
// nothing at all, silently.

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <Process/Dataflow/AudioPortComboBox.hpp>
#include <Process/Dataflow/PortType.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
Device::FreeNodeList deviceDrop(const QString& name)
{
  Device::DeviceSettings s;
  s.name = name;
  return {{State::Address{name, {}}, Device::Node{s, nullptr}}};
}

Device::FreeNodeList parameterDrop(const QString& device, const QString& param)
{
  Device::AddressSettings a;
  a.name = param;
  return {{State::Address{device, {param}}, Device::Node{a, nullptr}}};
}
}

TEST_CASE("A device dropped on a port addressed by device names that device", "[port]")
{
  using namespace Process;
  for(auto type : {PortType::Audio, PortType::Midi, PortType::Texture, PortType::Geometry})
  {
    const auto addr = droppedDeviceAddress(deviceDrop("stagewindow"), type);
    REQUIRE(addr.has_value());
    CHECK(addr->device == QStringLiteral("stagewindow"));

    // Device and nothing else: this is exactly what makeDeviceCombo writes, so
    // dropping and choosing from the list leave the port in the same state.
    CHECK(addr->path.isEmpty());
  }
}

TEST_CASE("A message port is not satisfied by a device", "[port]")
{
  // It needs a parameter to read or write. Naming a device alone would leave a
  // port pointing at something with no value.
  CHECK_FALSE(
      Process::droppedDeviceAddress(deviceDrop("stagewindow"), Process::PortType::Message)
          .has_value());
}

TEST_CASE("A parameter is left to the path that handles parameters", "[port]")
{
  // Dropping an address carries its AddressSettings, and the port takes those
  // as well as the address -- domain, unit, type. That path is unaffected.
  CHECK_FALSE(Process::droppedDeviceAddress(
                  parameterDrop("stagewindow", "size"), Process::PortType::Texture)
                  .has_value());
}

TEST_CASE("An empty drop names nothing", "[port]")
{
  CHECK_FALSE(Process::droppedDeviceAddress({}, Process::PortType::Texture).has_value());

  // A node that claims to be a device but names none is not a device to point at.
  Device::DeviceSettings s;
  Device::FreeNodeList nameless{{State::Address{}, Device::Node{s, nullptr}}};
  CHECK_FALSE(
      Process::droppedDeviceAddress(nameless, Process::PortType::Texture).has_value());
}
