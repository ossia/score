#pragma once
#include <Scenario/Document/Event/EventModel.hpp>
#include <Network/Node.h>
#include <OSSIA/LocalTree/BaseProperty.hpp>
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <OSSIA/LocalTree/NameProperty.hpp>

namespace OSSIA
{
namespace LocalTree
{
class EventComponent final :
        public iscore::Component
{

    public:
        using system_t = OSSIA::LocalTree::DocumentPlugin;

        EventComponent(
                OSSIA::Node& parent,
                const Id<Component>& id,
                EventModel& event,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_comp);

        const Key& key() const override;

        auto& node() const
        { return m_thisNode.node; }

        ~EventComponent();

    private:
        OSSIA::Node& thisNode() const
        { return *node(); }

        MetadataNamePropertyWrapper m_thisNode;
        std::vector<std::unique_ptr<BaseProperty>> m_properties;
};
}
}
