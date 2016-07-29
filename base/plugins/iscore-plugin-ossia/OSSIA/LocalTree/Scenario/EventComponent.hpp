#pragma once
#include <Scenario/Document/Event/EventModel.hpp>
#include <OSSIA/LocalTree/LocalTreeComponent.hpp>
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>

namespace Ossia
{
namespace LocalTree
{
class Event final :
        public CommonComponent
{
        COMPONENT_METADATA("918b757d-d304-4362-bbd3-120f84e229fe")
    public:
        Event(
                ossia::net::Node& parent,
                const Id<iscore::Component>& id,
                Scenario::EventModel& event,
                DocumentPlugin& doc,
                QObject* parent_comp);
};
}
}
