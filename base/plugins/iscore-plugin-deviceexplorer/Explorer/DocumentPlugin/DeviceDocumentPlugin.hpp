#pragma once
#include <Device/Protocol/DeviceList.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/DocumentPlugin/ListeningState.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>

#include <core/document/DocumentContext.hpp>

#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

class DeviceDocumentPlugin final : public iscore::DocumentDelegatePluginModel
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

        iscore::Node& rootNode()
        { return m_rootNode; }
        const iscore::Node& rootNode() const
        { return m_rootNode; }

        DeviceList& list()
        { return m_list; }

        const DeviceList& list() const
        { return m_list; }

        // TODO make functions that take an address and call list().device(...).TheRelevantMethod

        iscore::Node createDeviceFromNode(const iscore::Node&);
        iscore::Node loadDeviceFromNode(const iscore::Node&);

        NodeUpdateProxy updateProxy{*this};

        ListeningState pauseListening();
        // Note : if the tree has changed in between, we should make a new ListeningState
        void resumeListening(const ListeningState&);

        void setConnection(bool);

    private:
        iscore::Node m_rootNode;
        DeviceList m_list;
};

