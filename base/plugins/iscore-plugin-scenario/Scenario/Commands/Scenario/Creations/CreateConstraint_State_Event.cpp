#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/tools/RandomNameProvider.hpp>

#include <QByteArray>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <vector>

#include "CreateConstraint_State_Event.hpp"
#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>

namespace Scenario
{
namespace Command
{
CreateConstraint_State_Event::CreateConstraint_State_Event(
    const Scenario::ProcessModel& scenario,
    Id<StateModel>
        startState,
    Id<TimeNodeModel>
        endTimeNode,
    double endStateY)
    : m_newEvent{getStrongId(scenario.events)}
    , m_createdName{RandomNameProvider::generateRandomName()}
    , m_command{scenario, std::move(startState), m_newEvent, endStateY}
    , m_endTimeNode{std::move(endTimeNode)}
{
}

void CreateConstraint_State_Event::undo(const iscore::DocumentContext& ctx) const
{
  m_command.undo(ctx);

  ScenarioCreate<EventModel>::undo(
      m_newEvent, m_command.scenarioPath().find(ctx));
}

void CreateConstraint_State_Event::redo(const iscore::DocumentContext& ctx) const
{
  auto& scenar = m_command.scenarioPath().find(ctx);

  // Create the end event
  ScenarioCreate<EventModel>::redo(
      m_newEvent,
      scenar.timeNode(m_endTimeNode),
      {m_command.endStateY(), m_command.endStateY()},
      scenar);

  scenar.events.at(m_newEvent).metadata().setName(m_createdName);

  // The state + constraint between
  m_command.redo(ctx);
}

void CreateConstraint_State_Event::serializeImpl(DataStreamInput& s) const
{
  s << m_newEvent << m_createdName << m_command.serialize() << m_endTimeNode;
}

void CreateConstraint_State_Event::deserializeImpl(DataStreamOutput& s)
{
  QByteArray b;
  s >> m_newEvent >> m_createdName >> b >> m_endTimeNode;

  m_command.deserialize(b);
}
}
}
