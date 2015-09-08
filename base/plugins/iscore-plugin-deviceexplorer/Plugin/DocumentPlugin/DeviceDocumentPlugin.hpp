#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

#include <iscore/serialization/VisitorCommon.hpp>
#include <DeviceExplorer/Protocol/DeviceList.hpp>
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>
#include "NodeUpdateProxy.hpp"
namespace iscore
{
    class Document;
}



class DeviceDocumentPlugin : public iscore::DocumentDelegatePluginModel
{
        Q_OBJECT
    public:
        explicit DeviceDocumentPlugin(QObject* parent);
        DeviceDocumentPlugin(const VisitorVariant& loader,
                             QObject* parent);

        void serialize(const VisitorVariant&) const override;

        iscore::Node& rootNode()
        { return m_rootNode; }

        DeviceList& list()
        { return m_list; }

        const DeviceList& list() const
        { return m_list; }

        // TODO make functions that take an address and call list().device(...).TheRelevantMethod

        static void addNodeToDevice(DeviceInterface& dev, iscore::Node& node);
        iscore::Node createDeviceFromNode(const iscore::Node&);

        NodeUpdateProxy updateProxy{*this};

    private:
        iscore::Node m_rootNode;
        DeviceList m_list;
};

