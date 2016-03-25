#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include <Explorer/DocumentPlugin/ListeningState.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/Listening/ListeningHandler.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore_plugin_deviceexplorer_export.h>
#include <core/document/Document.hpp>

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
        Device::Node loadDeviceFromNode(const Device::Node&);

        NodeUpdateProxy updateProxy{*this};

        void setConnection(bool);

        Explorer::ListeningHandler& listening() const;

    private:
        void serialize_impl(const VisitorVariant&) const override;
        ConcreteFactoryKey concreteFactoryKey() const override;

        void initDevice(Device::DeviceInterface&);
        Device::Node m_rootNode;
        Device::DeviceList m_list;

        mutable std::unique_ptr<Explorer::ListeningHandler> m_listening;
};

}
