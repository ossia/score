// Unit test: a device's node callbacks across disconnect / reconnect.
//
// DeviceInterface::disconnect() clears the ossia tree. The node callbacks
// enableCallbacks() installed (on_node_removing -> pathRemoved...) must be
// gone by then, whatever the device's hasCallbacks capability says: the
// Window device advertises no callbacks yet installs them in reconnect(), and
// used to report every one of its nodes as removed on each disconnect - the
// explorer then dropped the device's whole tree (on every document switch,
// which disconnects and reconnects all devices).

#include <State/Address.hpp>

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>

#include <QObject>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

namespace
{
// Like the Window device: no callbacks advertised, its own tree rebuilt on
// every reconnect, callbacks installed all the same.
struct WindowLikeDevice final : Device::DeviceInterface
{
  std::unique_ptr<ossia::net::generic_device> dev;

  WindowLikeDevice()
      : Device::DeviceInterface{[] {
        Device::DeviceSettings s;
        s.name = "window";
        return s;
      }()}
  {
    m_capas.hasCallbacks = false;
  }

  bool reconnect() override
  {
    disconnect();
    dev = std::make_unique<ossia::net::generic_device>(
        std::make_unique<ossia::net::multiplex_protocol>(), "window");
    ossia::net::find_or_create_node(dev->get_root_node(), "/size");
    ossia::net::find_or_create_node(dev->get_root_node(), "/fullscreen");
    enableCallbacks();
    deviceChanged(nullptr, dev.get());
    return true;
  }

  void disconnect() override
  {
    Device::DeviceInterface::disconnect();
    auto prev = std::move(dev);
    deviceChanged(prev.get(), nullptr);
  }

  ossia::net::device_base* getDevice() const override { return dev.get(); }
};
}

TEST_CASE("Disconnecting a device does not report its nodes as removed", "[device][callbacks]")
{
  WindowLikeDevice dev;
  std::vector<State::Address> removed, added;
  QObject::connect(
      &dev, &Device::DeviceInterface::pathRemoved, &dev,
      [&](const State::Address& a) { removed.push_back(a); });
  QObject::connect(
      &dev, &Device::DeviceInterface::pathAdded, &dev,
      [&](const State::Address& a) { added.push_back(a); });

  REQUIRE(dev.reconnect());
  REQUIRE(dev.getDevice()->get_root_node().children().size() == 2);

  dev.disconnect();
  CHECK(removed.empty());
  CHECK(dev.getDevice() == nullptr);
}

TEST_CASE("A device rebuilt by reconnect() gets its callbacks back", "[device][callbacks]")
{
  WindowLikeDevice dev;
  std::vector<State::Address> removed, added;
  QObject::connect(
      &dev, &Device::DeviceInterface::pathRemoved, &dev,
      [&](const State::Address& a) { removed.push_back(a); });
  QObject::connect(
      &dev, &Device::DeviceInterface::pathAdded, &dev,
      [&](const State::Address& a) { added.push_back(a); });

  REQUIRE(dev.reconnect());
  // As a document switch does: tear everything down, build it again
  REQUIRE(dev.reconnect());
  CHECK(removed.empty());

  // The second device is watched: what it creates or drops is reported
  added.clear();
  auto& root = dev.getDevice()->get_root_node();
  auto& child = ossia::net::find_or_create_node(root, "/title");
  REQUIRE(added.size() == 1);
  CHECK(added.front().toString() == "window:/title");

  root.remove_child(child);
  REQUIRE(removed.size() == 1);
  CHECK(removed.front().toString() == "window:/title");

  // ... and a disconnect is still quiet
  removed.clear();
  dev.disconnect();
  CHECK(removed.empty());
}
