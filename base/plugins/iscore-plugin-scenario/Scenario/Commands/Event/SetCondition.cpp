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
    Path<EventModel>&& eventPath, State::Expression&& cond)
    : m_path{std::move(eventPath)}, m_condition(std::move(cond))
{

  auto& event = m_path.find();
  m_previousCondition = event.condition();
}

void SetCondition::undo() const
{
  auto& event = m_path.find();
  event.setCondition(m_previousCondition);
}

void SetCondition::redo() const
{
  auto& event = m_path.find();
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
    Path<EventModel>&& path, OffsetBehavior newval)
    : iscore::PropertyCommand{std::move(path), "offsetBehavior",
                              QVariant::fromValue(newval)}
{
}
}
}
