#pragma once
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Network/Node.h>
#include <OSSIA/LocalTree/BaseProperty.hpp>
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <OSSIA/LocalTree/NameProperty.hpp>


namespace OSSIA
{
namespace LocalTree
{
class TimeNodeComponent final :
        public iscore::Component
{
    public:
        using system_t = OSSIA::LocalTree::DocumentPlugin;

        TimeNodeComponent(
                OSSIA::Node& parent,
                const Id<iscore::Component>& id,
                TimeNodeModel& timeNode,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_comp);

        const Key& key() const override;

        auto& node() const
        { return m_thisNode.node; }
        ~TimeNodeComponent();

    private:
        MetadataNamePropertyWrapper m_thisNode;
        std::vector<BaseProperty*> m_properties;

        OSSIA::Node& thisNode() const
        { return *node(); }
};
}
}
