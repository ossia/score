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

class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT DeviceDocumentPlugin final : public iscore::DocumentPluginModel
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

        void serialize(const VisitorVariant&) const override;

        Device::Node& rootNode()
        { return m_rootNode; }
        const Device::Node& rootNode() const
        { return m_rootNode; }

        DeviceList& list()
        { return m_list; }

        const DeviceList& list() const
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
        void initDevice(DeviceInterface&);
        Device::Node m_rootNode;
        DeviceList m_list;
};

