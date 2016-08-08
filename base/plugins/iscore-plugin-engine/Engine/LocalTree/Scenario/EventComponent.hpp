#pragma once
#include <Scenario/Document/Event/EventModel.hpp>
#include <Engine/LocalTree/LocalTreeComponent.hpp>
#include <Engine/LocalTree/LocalTreeDocumentPlugin.hpp>

namespace Engine
{
namespace LocalTree
{
class Event final :
        public CommonComponent
{
        COMPONENT_METADATA("918b757d-d304-4362-bbd3-120f84e229fe")
    public:
        Event(
                ossia::net::node_base& parent,
                const Id<iscore::Component>& id,
                Scenario::EventModel& event,
                DocumentPlugin& doc,
                QObject* parent_comp);
};
}
}
