#include "EventComponent.hpp"
#include "MetadataParameters.hpp"

namespace Ossia
{
namespace LocalTree
{
EventComponent::EventComponent(
        OSSIA::Node& parent,
        const Id<iscore::Component>& id,
        Scenario::EventModel& event,
        const EventComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent_comp):
    Component{id, "EventComponent", parent_comp},
    m_thisNode{parent, event.metadata, this}
{
    make_metadata_node(event.metadata, thisNode(), m_properties, this);
}

const iscore::Component::Key&EventComponent::key() const
{
    static const Key k{"Ossia::LocalTree::EventComponent"};
    return k;
}

EventComponent::~EventComponent()
{
}

}
}
