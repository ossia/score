#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <QObject>
#include <QString>
#include <State/Address.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <vector>

#include <State/Value.hpp>
#include <iscore_lib_device_export.h>
#include <nano_signal_slot.hpp>
namespace ossia
{
class value;
}

namespace State
{
struct Message;
}
namespace Device
{
struct FullAddressSettings;

struct ISCORE_LIB_DEVICE_EXPORT DeviceCapas
{
  bool canAddNode{true};
  bool canRemoveNode{true};
  bool canRenameNode{true};
  bool canDisconnect{true};
  bool canRefreshValue{true};
  bool canRefreshTree{false};
  bool canListen{true};
  bool canSerialize{true};
  bool canLearn{false};
  bool hasCallbacks{true};
};

class ISCORE_LIB_DEVICE_EXPORT DeviceInterface : public QObject
{
  Q_OBJECT

public:
  explicit DeviceInterface(Device::DeviceSettings s);
  virtual ~DeviceInterface();

  const Device::DeviceSettings& settings() const;
  const auto& name() const
  {
    return settings().name;
  }

  virtual void addNode(const Device::Node& n);

  DeviceCapas capabilities() const
  {
    return m_capas;
  }

  virtual void disconnect() = 0;
  virtual bool reconnect() = 0;
  virtual void recreate(const Device::Node&) {} // Argument is the node of the device, used for recreation
  virtual bool connected() const = 0;

  virtual void updateSettings(const Device::DeviceSettings&) = 0;

  // Asks, and returns all the new addresses if the device can refresh itself
  // Minuit-like.
  // The addresses are not applied to the device, they have to be via a
  // command!
  virtual Device::Node refresh()
  {
    return {};
  }
  virtual optional<State::Value> refresh(const State::Address&)
  {
    return {};
  }
  virtual void request(const State::Address&)
  {
  }
  virtual void setListening(const State::Address&, bool)
  {
  }
  virtual void addToListening(const std::vector<State::Address>&)
  {
  }
  virtual std::vector<State::Address> listening() const
  {
    return {};
  }

  virtual void addAddress(const Device::FullAddressSettings&) = 0;
  virtual void updateAddress(
      const State::Address& currentAddr,
      const Device::FullAddressSettings& newAddr)
      = 0;
  virtual void removeNode(const State::Address&) = 0;

  // Execution API... Maybe we don't need it here.
  virtual void sendMessage(const State::Message& mess) = 0;

  // Make a node from an inside path, if it has been added for instance.
  virtual Device::Node getNode(const State::Address&) = 0;
  virtual Device::Node getNodeWithoutChildren(const State::Address&) = 0;

  virtual bool isLogging() const = 0;
  virtual void setLogging(bool) = 0;

  virtual bool isLearning() const
  {
    return false;
  }
  virtual void setLearning(bool)
  {
  }

  Nano::Signal<void(const State::Address&, const ossia::value&)> valueUpdated;

signals:
  // These signals are emitted if a device changes from the inside
  void pathAdded(const State::Address&);
  void pathUpdated(
      const State::Address&,           // current address
      const Device::AddressSettings&); // new data
  void pathRemoved(const State::Address&);

  // In case the whole namespace changed?
  void namespaceUpdated();

  /* If logging is enabled, these two signals may be sent
   * when something happens */
  void logInbound(const QString&) const;
  void logOutbound(const QString&) const;

protected:
  Device::DeviceSettings m_settings;
  DeviceCapas m_capas;
};
}
