#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include <Explorer/DocumentPlugin/ListeningState.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore_plugin_deviceexplorer_export.h>
#include <core/document/Document.hpp>

class QObject;
namespace iscore {
class Document;
}  // namespace iscore
struct VisitorVariant;

namespace DeviceExplorer
{
class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT DeviceDocumentPlugin final :
        public iscore::SerializableDocumentPlugin,
        public iscore::concrete
{
        Q_OBJECT
    public:
        explicit DeviceDocumentPlugin(
                iscore::Document& documentContext,
                QObject* parent);

        DeviceDocumentPlugin(
                iscore::Document&,
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

        ListeningState pauseListening();
        // Note : if the tree has changed in between, we should make a new ListeningState
        void resumeListening(const ListeningState&);

        void setConnection(bool);

    private:
        void serialize_impl(const VisitorVariant&) const override;
        ConcreteFactoryKey concreteFactoryKey() const override;

        void initDevice(Device::DeviceInterface&);
        Device::Node m_rootNode;
        Device::DeviceList m_list;
};

class DocumentPluginFactory final :
        public iscore::DocumentPluginFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("6e610e1f-9de2-4c36-90dd-0ef570002a21")

    public:
        iscore::DocumentPlugin* load(
                const VisitorVariant& var,
                iscore::DocumentContext& doc,
                QObject* parent) override
        {
            // TODO smell
            return new DeviceDocumentPlugin{doc.document, var, parent};
        }
};
}
