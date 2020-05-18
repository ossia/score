#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <ossia-qt/device_metatype.hpp>
#include <ossia/detail/callback_container.hpp>
#include <ossia/network/base/value_callback.hpp>

#include <nano_observer.hpp>
#include <score_lib_device_export.h>

#include <verdigris>
class QMenu;
namespace ossia
{
class value;
}

namespace ossia::net
{
class node_base;
class parameter_base;
class device_base;
}
namespace State
{
struct Message;
}
namespace Device
{
struct FullAddressSettings;

struct SCORE_LIB_DEVICE_EXPORT DeviceCapas
{
  bool canAddNode{true};
  bool canRemoveNode{true};
  bool canRenameNode{true};
  bool canSetProperties{true};
  bool canDisconnect{true};
  bool canRefreshValue{true};
  bool canRefreshTree{false};
  bool asyncConnect{false};
  bool canListen{true};
  bool canSerialize{true};
  bool canLearn{false};
  bool hasCallbacks{true};
};

enum DeviceLogging : int8_t
{
  LogNothing,
  LogUnfolded,
  LogEverything
};
class SCORE_LIB_DEVICE_EXPORT DeviceInterface : public QObject, public Nano::Observer
{
  W_OBJECT(DeviceInterface)

public:
  explicit DeviceInterface(Device::DeviceSettings s);
  virtual ~DeviceInterface();

  const Device::DeviceSettings& settings() const;
  const QString& name() const;

  virtual void addNode(const Device::Node& n);

  DeviceCapas capabilities() const;

  virtual void disconnect();
  virtual bool reconnect() = 0;
  virtual void recreate(const Device::Node&); // Argument is the node of the
                                              // device, used for recreation
  virtual bool connected() const;

  void updateSettings(const Device::DeviceSettings&);

  // Asks, and returns all the new addresses if the device can refresh itself
  // Minuit-like.
  // The addresses are not applied to the device, they have to be via a
  // command!
  virtual Device::Node refresh();
  optional<ossia::value> refresh(const State::Address&);
  void request(const Device::Node&);
  void setListening(const State::Address&, bool);
  void addToListening(const std::vector<State::Address>&);
  std::vector<State::Address> listening() const;

  virtual void addAddress(const Device::FullAddressSettings&);
  virtual void
  updateAddress(const State::Address& currentAddr, const Device::FullAddressSettings& newAddr);
  void removeNode(const State::Address&);

  void sendMessage(const State::Address& addr, const ossia::value& v);

  // Make a node from an inside path, if it has been added for instance.
  Device::Node getNode(const State::Address&);
  Device::Node getNodeWithoutChildren(const State::Address&);

  bool isLogging() const;
  void setLogging(DeviceLogging);

  virtual ossia::net::device_base* getDevice() const = 0;

  virtual bool isLearning() const;
  virtual void setLearning(bool);

  virtual QMimeData* mimeData() const;
  virtual void setupContextMenu(QMenu&) const;

  void nodeCreated(const ossia::net::node_base&);
  void nodeRemoving(const ossia::net::node_base&);
  void nodeRenamed(const ossia::net::node_base&, std::string);
  void addressCreated(const ossia::net::parameter_base&);
  void addressUpdated(const ossia::net::node_base&, ossia::string_view key);
  void addressRemoved(const ossia::net::parameter_base& addr);

  Nano::Signal<void(const State::Address&, const ossia::value&)> valueUpdated;

public:
  // These signals are emitted if a device changes from the inside
  void pathAdded(const State::Address& arg_1) E_SIGNAL(SCORE_LIB_DEVICE_EXPORT, pathAdded, arg_1)
  void pathUpdated(
      const State::Address& arg_1, // current address
      const Device::AddressSettings& arg_2)
      E_SIGNAL(SCORE_LIB_DEVICE_EXPORT, pathUpdated, arg_1, arg_2) // new data
  void pathRemoved(const State::Address& arg_1)
      E_SIGNAL(SCORE_LIB_DEVICE_EXPORT, pathRemoved, arg_1)

  // In case the whole namespace changed?
  void namespaceUpdated() E_SIGNAL(SCORE_LIB_DEVICE_EXPORT, namespaceUpdated)

  // In case the device changed
  void deviceChanged(ossia::net::device_base* old_dev, ossia::net::device_base* new_dev)
      E_SIGNAL(SCORE_LIB_DEVICE_EXPORT, deviceChanged, old_dev, new_dev)

  /* If logging is enabled, these two signals may be sent
   * when something happens */
  void logInbound(const QString& arg_1) const E_SIGNAL(SCORE_LIB_DEVICE_EXPORT, logInbound, arg_1)
  void logOutbound(const QString& arg_1) const
      E_SIGNAL(SCORE_LIB_DEVICE_EXPORT, logOutbound, arg_1)

  void connectionChanged(bool arg_1) const
      E_SIGNAL(SCORE_LIB_DEVICE_EXPORT, connectionChanged, arg_1)

protected:
  Device::DeviceSettings m_settings;
  DeviceCapas m_capas;

  using callback_pair = std::pair<
      ossia::net::parameter_base*,
      ossia::callback_container<ossia::value_callback>::iterator>;
  score::hash_map<State::Address, callback_pair> m_callbacks;

  void removeListening_impl(ossia::net::node_base& node, State::Address addr);
  void removeListening_impl(
      ossia::net::node_base& node,
      State::Address addr,
      std::vector<State::Address>&);
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

class SCORE_LIB_DEVICE_EXPORT OwningDeviceInterface : public DeviceInterface
{
public:
  ~OwningDeviceInterface() override;
  void replaceDevice(ossia::net::device_base*);
  void releaseDevice();

protected:
  void disconnect() override;

  using DeviceInterface::DeviceInterface;

  ossia::net::device_base* getDevice() const final override { return m_dev.get(); }

  std::unique_ptr<ossia::net::device_base> m_dev;
  bool m_owned{true};
};

SCORE_LIB_DEVICE_EXPORT ossia::net::node_base*
getNodeFromPath(const QStringList& path, ossia::net::device_base& dev);

SCORE_LIB_DEVICE_EXPORT ossia::net::node_base*
createNodeFromPath(const QStringList& path, ossia::net::device_base& dev);

SCORE_LIB_DEVICE_EXPORT Device::Node ToDeviceExplorer(const ossia::net::node_base& node);

SCORE_LIB_DEVICE_EXPORT ossia::net::node_base*
findNodeFromPath(const Device::Node& path, ossia::net::device_base& dev);

SCORE_LIB_DEVICE_EXPORT ossia::net::node_base*
findNodeFromPath(const QStringList& path, ossia::net::device_base& dev);
}
