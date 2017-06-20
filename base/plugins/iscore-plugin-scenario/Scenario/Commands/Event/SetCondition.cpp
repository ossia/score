#include <Scenario/Document/Event/EventModel.hpp>
#include <algorithm>

#include "SetCondition.hpp"
#include <State/Expression.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/tree/TreeNode.hpp>

namespace Scenario
{
namespace Command
{
SetCondition::SetCondition(
    const EventModel& event, State::Expression&& cond)
    : m_path{event}, m_condition(std::move(cond))
{
  m_previousCondition = event.condition();
}

void SetCondition::undo(const iscore::DocumentContext& ctx) const
{
  auto& event = m_path.find(ctx);
  event.setCondition(m_previousCondition);
}

void SetCondition::redo(const iscore::DocumentContext& ctx) const
{
  auto& event = m_path.find(ctx);
  event.setCondition(m_condition);
}

void SetCondition::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_condition << m_previousCondition;
}

void SetCondition::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_condition >> m_previousCondition;
}

SetOffsetBehavior::SetOffsetBehavior(
    const EventModel& event,
    OffsetBehavior newval)
    : iscore::PropertyCommand{event, "offsetBehavior",
                              QVariant::fromValue(newval)}
{
}
}
}
