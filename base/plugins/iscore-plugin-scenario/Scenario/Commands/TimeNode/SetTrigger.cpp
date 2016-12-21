#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <algorithm>

#include "SetTrigger.hpp"
#include <State/Expression.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/tree/TreeNode.hpp>

namespace Scenario
{
namespace Command
{

SetTrigger::SetTrigger(
    Path<TimeNodeModel>&& timeNodePath, State::Expression trigger)
    : m_path{std::move(timeNodePath)}, m_trigger(std::move(trigger))
{
  auto& tn = m_path.find();
  m_previousTrigger = tn.trigger()->expression();
}

void SetTrigger::undo() const
{
  auto& tn = m_path.find();
  tn.trigger()->setExpression(m_previousTrigger);
}

void SetTrigger::redo() const
{
  auto& tn = m_path.find();
  tn.trigger()->setExpression(m_trigger);
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
