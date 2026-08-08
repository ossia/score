#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceCatalog.hpp>
#include <Device/Protocol/DeviceInterface.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Listening/ListeningHandler.hpp>

#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

#include <ossia/detail/hash_map.hpp>

#include <score_plugin_deviceexplorer_export.h>

#include <functional>
#include <thread>
#include <verdigris>

namespace ossia::net
{
struct network_context;
using network_context_ptr = std::shared_ptr<network_context>;
}

UUID_METADATA(
    , score::DocumentPluginFactory, Explorer::DeviceDocumentPlugin,
    "6e610e1f-9de2-4c36-90dd-0ef570002a21")

namespace Explorer
{
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT DeviceDocumentPlugin final
    : public score::SerializableDocumentPlugin
    , public Nano::Observer
{
  W_OBJECT(DeviceDocumentPlugin)
  SCORE_SERIALIZE_FRIENDS

  MODEL_METADATA_IMPL_HPP(DeviceDocumentPlugin)
public:
  explicit DeviceDocumentPlugin(const score::DocumentContext& ctx, QObject* parent);

  virtual ~DeviceDocumentPlugin();
  template <typename Impl>
  DeviceDocumentPlugin(const score::DocumentContext& ctx, Impl& vis, QObject* parent)
      : score::SerializableDocumentPlugin{ctx, vis, parent}
  {
    init();
    vis.writeTo(*this);
  }

  void init();

  void on_documentClosing() override;

  Device::Node& rootNode() { return m_rootNode; }
  const Device::Node& rootNode() const { return m_rootNode; }

  Device::DeviceList& list() { return m_list; }

  const Device::DeviceList& list() const { return m_list; }

  // TODO make functions that take a parameter and call
  // list().device(...).TheRelevantMethod

  Device::Node createDeviceFromNode(const Device::Node&);

  // If the output is different that the input,
  // it means that the node has changes and the output should
  // be used.
  std::optional<Device::Node> loadDeviceFromNode(const Device::Node&);

  void setConnection(bool);

  Explorer::ListeningHandler& listening() const;

  DeviceExplorerModel& explorer() const { return *m_explorer; }

  void setupConnections(Device::DeviceInterface&, bool enabled);

  void reconnect(const QString&);

  /**
   * @brief Re-explores the device's namespace and shows the result in the
   * explorer, in place of the device's current tree.
   *
   * Only for devices which can refresh their tree. If the exploration yields
   * nothing (device unreachable, namespace not received in time), the current
   * tree is kept rather than replaced by nothing.
   * Returns true if the tree was replaced.
   */
  bool refreshDeviceTree(Device::DeviceInterface& dev);

  /**
   * @brief Refreshes the device's tree once it has (re)connected.
   *
   * Used after the device's settings changed: the device reconnects
   * asynchronously with its new settings, replays its previous nodes
   * (DeviceInterface::recreate) and the namespace is then re-explored so that
   * the explorer shows what is actually there. Requests are coalesced: a
   * device has at most one refresh pending.
   */
  void refreshDeviceTreeOnReconnect(Device::DeviceInterface& dev);

  const ossia::net::network_context_ptr& networkContext() const noexcept
  {
    return m_asioContext;
  }

  //! What may be added, and whose hardware is offered. Null for an ordinary
  //! document, where it is this machine's.
  Device::DeviceCatalog* catalog() const noexcept { return m_catalog; }
  void setCatalog(Device::DeviceCatalog* c) noexcept { m_catalog = c; }

  //! Whether a device is connected, as reported by the machine that has it.
  //! Empty for an ordinary document, where the device itself is the answer.
  std::optional<bool> remoteConnected(const QString& device) const noexcept;
  void setRemoteConnected(const QString& device, bool connected);

  //! Devices the machine running the score reported as being of this kind.
  //! Empty for an ordinary document, where the objects can be asked directly.
  std::vector<QString> remoteDevicesOfKind(Device::DeviceKind kind) const;
  void setRemoteKinds(const QString& device, Device::DeviceKinds kinds);

  //! Where a value edited here goes when the device is on another machine.
  //! Unset for an ordinary document, which has the device to send to.
  using ValueSink = std::function<void(const State::Address&, const ossia::value&)>;
  void setValueSink(ValueSink s) { m_valueSink = std::move(s); }
  const ValueSink& valueSink() const noexcept { return m_valueSink; }

  //! Told what one of this machine's devices reported, as opposed to what
  //! somebody asked for. A peer that does not run the score has no device to
  //! hear it from, so this is the only way it learns a value moved.
  void setValueObserver(ValueSink s) { m_valueObserver = std::move(s); }

  //! The peer reported what a device is; arrives after the join.
  void remoteKindsChanged(const QString& device)
      E_SIGNAL(SCORE_PLUGIN_DEVICEEXPLORER_EXPORT, remoteKindsChanged, device)

  //! A device's own tree changed here: refreshed, or discovered something.
  //! What is inside a device is known only where the device is.
  void deviceTreeChanged(const QString& device)
      E_SIGNAL(SCORE_PLUGIN_DEVICEEXPLORER_EXPORT, deviceTreeChanged, device)

  //! One of this machine's devices reported a value. Called by the device
  //! itself, through the callback installed when it is opened.
  void on_valueUpdated(const State::Address& addr, const ossia::value& v);

private:
  void initDevice(Device::DeviceInterface&);

  Device::Node m_rootNode;
  Device::DeviceCatalog* m_catalog{};
  ValueSink m_valueSink;
  ValueSink m_valueObserver;
  ossia::hash_map<QString, bool> m_remoteConnected;
  ossia::hash_map<QString, Device::DeviceKinds> m_remoteKinds;
  Device::DeviceList m_list;
  std::atomic_bool m_processMessages{};
  std::thread m_asioThread;
  ossia::net::network_context_ptr m_asioContext;

  mutable std::unique_ptr<Explorer::ListeningHandler> m_listening;
  DeviceExplorerModel* m_explorer{};
  ossia::hash_map<Device::DeviceInterface*, std::vector<QMetaObject::Connection>>
      m_connections;
  // Devices with a refreshDeviceTreeOnReconnect() pending.
  ossia::hash_set<Device::DeviceInterface*> m_pendingTreeRefresh;
  // Devices with a deferred refreshDeviceTree() already queued.
  ossia::hash_set<Device::DeviceInterface*> m_queuedTreeRefresh;

  void asyncConnect(Device::DeviceInterface& newdev);
  void timerEvent(QTimerEvent* event) override;

public:
  NodeUpdateProxy updateProxy{*this};
};
}
