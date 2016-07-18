#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceList.hpp>

#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/Listening/ListeningHandler.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore_plugin_deviceexplorer_export.h>
#include <core/document/Document.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

class QObject;
namespace iscore {
class Document;
}  // namespace iscore
struct VisitorVariant;

namespace Explorer
{
class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT DeviceDocumentPlugin final :
        public iscore::SerializableDocumentPlugin,
        public iscore::concrete
{
        Q_OBJECT
    public:
        explicit DeviceDocumentPlugin(
                const iscore::DocumentContext& ctx,
                QObject* parent);

        DeviceDocumentPlugin(
                const iscore::DocumentContext& ctx,
                const VisitorVariant& loader,
                QObject* parent);

        Device::Node& rootNode()
        { return m_rootNode; }
        const Device::Node& rootNode() const
        { return m_rootNode; }

        Device::DeviceList& list()
        { return m_list; }

        const Device::DeviceList& list() const
        { return m_list; }

        // TODO make functions that take an address and call list().device(...).TheRelevantMethod

        Device::Node createDeviceFromNode(const Device::Node&);

        // If the output is different that the input,
        // it means that the node has changes and the output should
        // be used.
        optional<Device::Node> loadDeviceFromNode(const Device::Node&);


        void setConnection(bool);

        Explorer::ListeningHandler& listening() const;

    private:
        void serialize_impl(const VisitorVariant&) const override;
        ConcreteFactoryKey concreteFactoryKey() const override;

        void initDevice(Device::DeviceInterface&);
        Device::Node m_rootNode;
        Device::DeviceList m_list;

        mutable std::unique_ptr<Explorer::ListeningHandler> m_listening;

    public:
        DeviceExplorerModel explorer{*this, this};
        NodeUpdateProxy updateProxy{*this};
        Device::Node m_loadingNode; // FIXME hack
        // TODO maybe have another root node only for the "local" device ?
        // Also have something to go through the document recursively if a node changes....
        // or have "local" addresses with a pointer to something instead of a textual address.
};

}
