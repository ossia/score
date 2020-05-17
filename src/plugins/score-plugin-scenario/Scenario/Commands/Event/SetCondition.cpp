// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SetCondition.hpp"

#include <Scenario/Document/Event/EventModel.hpp>
#include <State/Expression.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace Scenario
{
namespace Command
{
SetCondition::SetCondition(const EventModel& event, State::Expression&& cond)
    : m_path{event}, m_condition(std::move(cond)), m_previousCondition{event.condition()}
{
}

void SetCondition::undo(const score::DocumentContext& ctx) const
{
  auto& event = m_path.find(ctx);
  event.setCondition(m_previousCondition);
}

void SetCondition::redo(const score::DocumentContext& ctx) const
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

SetOffsetBehavior::SetOffsetBehavior(const EventModel& event, OffsetBehavior newval)
    : score::PropertyCommand{event, "offsetBehavior", QVariant::fromValue(newval)}
{
}
}
}
