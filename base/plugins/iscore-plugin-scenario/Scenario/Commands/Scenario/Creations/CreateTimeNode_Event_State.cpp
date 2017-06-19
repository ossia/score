#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <QByteArray>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <vector>

#include "CreateTimeNode_Event_State.hpp"
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateEvent_State.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>

namespace Scenario
{
namespace Command
{
CreateTimeNode_Event_State::CreateTimeNode_Event_State(
    const Scenario::ProcessModel& scenario,
    TimeVal date,
    double stateY)
    : m_newTimeNode{getStrongId(scenario.timeNodes)}
    , m_date{std::move(date)}
    , m_command{scenario, m_newTimeNode, stateY}
{
}

void CreateTimeNode_Event_State::undo(const iscore::DocumentContext& ctx) const
{
  m_command.undo(ctx);

  ScenarioCreate<TimeNodeModel>::undo(
      m_newTimeNode, m_command.scenarioPath().find(ctx));
}

void CreateTimeNode_Event_State::redo(const iscore::DocumentContext& ctx) const
{
  auto& scenar = m_command.scenarioPath().find(ctx);

  // Create the node
  ScenarioCreate<TimeNodeModel>::redo(
      m_newTimeNode, {0.4, 0.6}, m_date, scenar);

  // And the event
  m_command.redo(ctx);
}

void CreateTimeNode_Event_State::serializeImpl(DataStreamInput& s) const
{
  s << m_newTimeNode << m_date << m_command.serialize();
}

void CreateTimeNode_Event_State::deserializeImpl(DataStreamOutput& s)
{
  QByteArray b;
  s >> m_newTimeNode >> m_date >> b;

  m_command.deserialize(b);
}
}
}
