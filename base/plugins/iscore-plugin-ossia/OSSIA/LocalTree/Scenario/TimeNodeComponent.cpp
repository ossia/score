#include "TimeNodeComponent.hpp"
#include "MetadataParameters.hpp"
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>

namespace Ossia
{
namespace LocalTree
{

TimeNodeComponent::TimeNodeComponent(
        OSSIA::Node& parent,
        const Id<iscore::Component>& id,
        TimeNodeModel& timeNode,
        const TimeNodeComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent_comp):
    Component{id, "TimeNodeComponent", parent_comp},
    m_thisNode{parent, timeNode.metadata, this}
{
    make_metadata_node(timeNode.metadata, thisNode(), m_properties, this);

    m_properties.push_back(
    add_setProperty<State::impulse_t>(thisNode(), "trigger",
                                       [&] (auto) {
        timeNode.trigger()->triggered();
    }));
}

const iscore::Component::Key&TimeNodeComponent::key() const
{
    static const Key k{"Ossia::LocalTree::TimeNodeComponent"};
    return k;
}

TimeNodeComponent::~TimeNodeComponent()
{
}

}
}
