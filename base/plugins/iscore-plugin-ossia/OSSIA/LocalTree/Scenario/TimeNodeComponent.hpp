#pragma once
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Network/Node.h>
#include <OSSIA/LocalTree/BaseProperty.hpp>
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <OSSIA/LocalTree/NameProperty.hpp>


namespace Ossia
{
namespace LocalTree
{
class TimeNode final :
        public iscore::Component
{
    public:
        using system_t = Ossia::LocalTree::DocumentPlugin;

        TimeNode(
                OSSIA::Node& parent,
                const Id<iscore::Component>& id,
                Scenario::TimeNodeModel& timeNode,
                const system_t& doc,
                QObject* parent_comp);

        const Key& key() const override;

        auto& node() const
        { return m_thisNode.node; }
        ~TimeNode();

    private:
        MetadataNamePropertyWrapper m_thisNode;
        std::vector<std::unique_ptr<BaseProperty>> m_properties;

        OSSIA::Node& thisNode() const
        { return *node(); }
};
}
}
