// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SetTrigger.hpp"

#include <State/Expression.hpp>

#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeSerialization.hpp>

namespace Scenario
{
namespace Command
{

SetTrigger::SetTrigger(const TimeSyncModel& tn, State::Expression trigger)
    : m_path{std::move(tn)}
    , m_trigger(std::move(trigger))
{
  m_previousTrigger = tn.expression();
}

void SetTrigger::undo(const score::DocumentContext& ctx) const
{
  auto& tn = m_path.find(ctx);
  tn.setExpression(m_previousTrigger);
}

void SetTrigger::redo(const score::DocumentContext& ctx) const
{
  auto& tn = m_path.find(ctx);
  tn.setExpression(m_trigger);
}

void SetTrigger::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_trigger << m_previousTrigger;
}

void SetTrigger::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_trigger >> m_previousTrigger;
}
}
}
