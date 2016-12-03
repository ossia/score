#include "EventComponent.hpp"
#include "MetadataParameters.hpp"
#include <ossia/editor/state/state_element.hpp>

namespace Engine
{
namespace LocalTree
{
Event::Event(
    ossia::net::node_base& parent,
    const Id<iscore::Component>& id,
    Scenario::EventModel& event,
    DocumentPlugin& doc,
    QObject* parent_comp)
    : CommonComponent{parent, event.metadata(), doc,
                      id,     "EventComponent", parent_comp}
{
}
}
}
