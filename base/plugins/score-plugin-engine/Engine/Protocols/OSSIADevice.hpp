#pragma once
#include <ossia/detail/callback_container.hpp>
#include <ossia/network/base/value_callback.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <State/Address.hpp>
#include <score/tools/std/Optional.hpp>
#include <score_plugin_engine_export.h>

#include <nano_observer.hpp>
#include <QString>
#include <functional>
#include <memory>
#include <score/tools/std/HashMap.hpp>
#include <utility>
#include <vector>
namespace ossia
{
namespace net
{
class node_base;
class parameter_base;
class device_base;
}
}

namespace Engine
{
namespace Network
{
using DeviceLogging = Device::DeviceLogging;
class SCORE_PLUGIN_ENGINE_EXPORT OSSIADevice
    : public Device::DeviceInterface, public Nano::Observer
{
    Q_OBJECT
public:
  virtual ~OSSIADevice();
  void disconnect() override;
  bool connected() const final override;

  void updateSettings(const Device::DeviceSettings& settings) final override;

  void addAddress(const Device::FullAddressSettings& settings) override;
  void updateAddress(
      const State::Address& currentAddr,
      const Device::FullAddressSettings& address) final override;
  void removeNode(const State::Address& path) final override;

  Device::Node refresh() override;

  // throws std::runtime_error
  using Device::DeviceInterface::refresh;
  optional<ossia::value> refresh(const State::Address&) final override;
  void request(const Device::Node&) final override;

  Device::Node getNode(const State::Address&) final override;
  Device::Node getNodeWithoutChildren(const State::Address&) final override;

  void setListening(const State::Address&, bool) final override;

  std::vector<State::Address> listening() const final override;
  void addToListening(const std::vector<State::Address>&) final override;

  void sendMessage(const State::Message& mess) final override;

  bool isLogging() const final override;
  void setLogging(DeviceLogging) final override;

  virtual ossia::net::device_base* getDevice() const = 0;

  void nodeCreated(const ossia::net::node_base&);
  void nodeRemoving(const ossia::net::node_base&);
  void nodeRenamed(const ossia::net::node_base&, std::string);
  void addressCreated(const ossia::net::parameter_base&);
  void addressUpdated(const ossia::net::node_base&, ossia::string_view key);
  void addressRemoved(const ossia::net::parameter_base& addr);

protected:
  using DeviceInterface::DeviceInterface;

  using callback_pair = std::pair<ossia::net::parameter_base*, ossia::callback_container<ossia::value_callback>::iterator>;
  score::hash_map<State::Address, callback_pair> m_callbacks;

  void removeListening_impl(ossia::net::node_base& node, State::Address addr);
  void removeListening_impl(ossia::net::node_base& node, State::Address addr, std::vector<State::Address>&);
  void renameListening_impl(const State::Address& parent, const QString& newName);
  void setLogging_impl(DeviceLogging) const;
  void enableCallbacks();
  void disableCallbacks();

  // Refresh without handling callbacks
  Device::Node simple_refresh();


private:
  DeviceLogging m_logging = DeviceLogging::LogNothing;
  bool m_callbacksEnabled = false;
};

class SCORE_PLUGIN_ENGINE_EXPORT OwningOSSIADevice : public OSSIADevice
{
public:
  virtual ~OwningOSSIADevice();
  void replaceDevice(ossia::net::device_base*);
  void releaseDevice();

protected:
  void disconnect() override;

  using OSSIADevice::OSSIADevice;

  ossia::net::device_base* getDevice() const final override
  {
    return m_dev.get();
  }

  std::unique_ptr<ossia::net::device_base> m_dev;
  bool m_owned{true};
};

}
}
