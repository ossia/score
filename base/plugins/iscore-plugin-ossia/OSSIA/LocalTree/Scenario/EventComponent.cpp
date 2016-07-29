#include "EventComponent.hpp"
#include "MetadataParameters.hpp"

namespace Ossia
{
namespace LocalTree
{
Event::Event(
        ossia::net::Node& parent,
        const Id<iscore::Component>& id,
        Scenario::EventModel& event,
        DocumentPlugin& doc,
        QObject* parent_comp):
    CommonComponent{parent, event.metadata, doc, id, "EventComponent", parent_comp}
{
}
}
}
