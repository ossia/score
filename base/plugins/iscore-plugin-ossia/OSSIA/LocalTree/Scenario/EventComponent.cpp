#include "EventComponent.hpp"
#include "MetadataParameters.hpp"

namespace OSSIA
{
namespace LocalTree
{

EventComponent::EventComponent(
        Node& parent,
        const Id<iscore::Component>& id,
        EventModel& event,
        const EventComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent_comp):
    Component{id, "EventComponent", parent_comp},
    m_thisNode{add_node(parent, event.metadata.name().toStdString())}
{
    make_metadata_node(event.metadata, *m_thisNode, m_properties, this);
}

const iscore::Component::Key&EventComponent::key() const
{
    static const Key k{"OSSIA::LocalTree::EventComponent"};
    return k;
}

EventComponent::~EventComponent()
{
    for(auto prop : m_properties)
        delete prop;
}

}
}
