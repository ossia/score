#pragma once
#include <Scenario/Document/Event/EventModel.hpp>
#include <Network/Node.h>
#include <OSSIA/LocalTree/BaseProperty.hpp>
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>

namespace OSSIA
{
namespace LocalTree
{
class EventComponent final :
        public iscore::Component
{
        std::shared_ptr<OSSIA::Node> m_thisNode;
        std::vector<BaseProperty*> m_properties;

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
        { return m_thisNode; }
        ~EventComponent();
};
}
}
