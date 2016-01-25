#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include <Explorer/DocumentPlugin/ListeningState.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore_plugin_deviceexplorer_export.h>

class QObject;
namespace iscore {
class Document;
}  // namespace iscore
struct VisitorVariant;

namespace DeviceExplorer
{
class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT DeviceDocumentPlugin final :
        public iscore::DocumentPluginModel,
        public iscore::SerializableInterface
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
        iscore::uuid_t uuid() const override;

        void initDevice(Device::DeviceInterface&);
        Device::Node m_rootNode;
        Device::DeviceList m_list;
};
}
