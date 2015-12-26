#include "TimeNodeComponent.hpp"
#include "MetadataParameters.hpp"
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>

namespace OSSIA
{
namespace LocalTree
{

TimeNodeComponent::TimeNodeComponent(
        Node& parent,
        const Id<iscore::Component>& id,
        TimeNodeModel& timeNode,
        const TimeNodeComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent_comp):
    Component{id, "TimeNodeComponent", parent_comp},
    m_thisNode{add_node(parent, timeNode.metadata.name().toStdString())}
{
    make_metadata_node(timeNode.metadata, *m_thisNode, m_properties, this);

    m_properties.push_back(
    add_setProperty<iscore::impulse_t>(*m_thisNode, "trigger",
                                       [&] (auto) {
        timeNode.trigger()->triggered();
    }));
}

const iscore::Component::Key&TimeNodeComponent::key() const
{
    static const Key k{"OSSIA::LocalTree::TimeNodeComponent"};
    return k;
}

TimeNodeComponent::~TimeNodeComponent()
{
    for(auto prop : m_properties)
        delete prop;
}

}
}
