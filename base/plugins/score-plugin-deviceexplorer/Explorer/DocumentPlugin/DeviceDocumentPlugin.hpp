#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/DeviceList.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPluginFactory.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Listening/ListeningHandler.hpp>
#include <core/document/Document.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score_plugin_deviceexplorer_export.h>
namespace Explorer
{
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT DeviceDocumentPlugin final
    : public score::SerializableDocumentPlugin,
      public Nano::Observer
{
  Q_OBJECT
  SCORE_SERIALIZE_FRIENDS

  MODEL_METADATA_IMPL(DeviceDocumentPlugin)
public:
  explicit DeviceDocumentPlugin(
      const score::DocumentContext& ctx,
      Id<DocumentPlugin>
          id,
      QObject* parent);

  virtual ~DeviceDocumentPlugin();
  template <typename Impl>
  DeviceDocumentPlugin(
      const score::DocumentContext& ctx,
      Impl& vis,
      QObject* parent)
      : score::SerializableDocumentPlugin{ctx, vis, parent}
  {
    vis.writeTo(*this);
  }

  Device::Node& rootNode()
  {
    return m_rootNode;
  }
  const Device::Node& rootNode() const
  {
    return m_rootNode;
  }

  Device::DeviceList& list()
  {
    return m_list;
  }

  const Device::DeviceList& list() const
  {
    return m_list;
  }

  // TODO make functions that take a parameter and call
  // list().device(...).TheRelevantMethod

  Device::Node createDeviceFromNode(const Device::Node&);

  // If the output is different that the input,
  // it means that the node has changes and the output should
  // be used.
  optional<Device::Node> loadDeviceFromNode(const Device::Node&);

  void setConnection(bool);

  Explorer::ListeningHandler& listening() const;

  DeviceExplorerModel& explorer() const
  {
    return *m_explorer;
  }

private:
  void initDevice(Device::DeviceInterface&);
  void on_valueUpdated(const State::Address& addr, const ossia::value& v);

  Device::Node m_rootNode;
  Device::DeviceList m_list;

  mutable std::unique_ptr<Explorer::ListeningHandler> m_listening;
  DeviceExplorerModel* m_explorer{};

public:
  NodeUpdateProxy updateProxy{*this};
};
}
