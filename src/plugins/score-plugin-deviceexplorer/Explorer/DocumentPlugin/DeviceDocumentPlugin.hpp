#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceCatalog.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Listening/ListeningHandler.hpp>

#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

#include <ossia/detail/hash_map.hpp>

#include <score_plugin_deviceexplorer_export.h>

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

  const ossia::net::network_context_ptr& networkContext() const noexcept
  {
    return m_asioContext;
  }

  //! What may be added to this document, and whose hardware is offered.
  //!
  //! Null for an ordinary document: this machine's protocols and this
  //! machine's devices, which is what the dialogs have always shown. Set when
  //! the score runs elsewhere, so that what is offered is reachable by it.
  Device::DeviceCatalog* catalog() const noexcept { return m_catalog; }
  void setCatalog(Device::DeviceCatalog* c) noexcept { m_catalog = c; }

  //! Whether a device is connected, as reported by the machine that has it.
  //!
  //! A document whose score runs elsewhere has no DeviceInterface to ask, so
  //! every device would read as disconnected -- which is not unknown, it is
  //! wrong. Empty for an ordinary document, where the device itself is the
  //! answer.
  std::optional<bool> remoteConnected(const QString& device) const noexcept;
  void setRemoteConnected(const QString& device, bool connected);

private:
  void initDevice(Device::DeviceInterface&);
  void on_valueUpdated(const State::Address& addr, const ossia::value& v);

  Device::Node m_rootNode;
  Device::DeviceCatalog* m_catalog{};
  ossia::hash_map<QString, bool> m_remoteConnected;
  Device::DeviceList m_list;
  std::atomic_bool m_processMessages{};
  std::thread m_asioThread;
  ossia::net::network_context_ptr m_asioContext;

  mutable std::unique_ptr<Explorer::ListeningHandler> m_listening;
  DeviceExplorerModel* m_explorer{};
  ossia::hash_map<Device::DeviceInterface*, std::vector<QMetaObject::Connection>>
      m_connections;

  void asyncConnect(Device::DeviceInterface& newdev);
  void timerEvent(QTimerEvent* event) override;

public:
  NodeUpdateProxy updateProxy{*this};
};
}
