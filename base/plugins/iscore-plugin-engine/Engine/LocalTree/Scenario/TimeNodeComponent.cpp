// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TimeNodeComponent.hpp"
#include "MetadataParameters.hpp"
#include <ossia/editor/state/state_element.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <State/Value.hpp>

namespace Engine
{
namespace LocalTree
{
TimeNode::TimeNode(
    ossia::net::node_base& parent,
    const Id<iscore::Component>& id,
    Scenario::TimeNodeModel& timeNode,
    DocumentPlugin& doc,
    QObject* parent_comp)
    : CommonComponent{parent, timeNode.metadata(), doc,
                      id,     "StateComponent",    parent_comp}
{
  m_properties.push_back(add_setProperty<::State::impulse>(
      node(), "trigger", [&](auto) { timeNode.trigger()->triggeredByGui(); }));
}
}
}
