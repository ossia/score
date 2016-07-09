#include "TimeNodeComponent.hpp"
#include "MetadataParameters.hpp"
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>

namespace Ossia
{
namespace LocalTree
{

TimeNode::TimeNode(
        OSSIA::Node& parent,
        const Id<iscore::Component>& id,
        Scenario::TimeNodeModel& timeNode,
        const TimeNode::system_t& doc,
        QObject* parent_comp):
    Component{id, "TimeNodeComponent", parent_comp},
    m_thisNode{parent, timeNode.metadata, this}
{
    make_metadata_node(timeNode.metadata, thisNode(), m_properties, this);

    m_properties.push_back(
    add_setProperty<State::impulse_t>(thisNode(), "trigger",
                                       [&] (auto) {
        timeNode.trigger()->triggeredByGui();
    }));
}

const iscore::Component::Key&TimeNode::key() const
{
    static const Key k{"Ossia::LocalTree::TimeNodeComponent"};
    return k;
}

TimeNode::~TimeNode()
{
    m_properties.clear();
    m_thisNode.clear();
}

}
}
