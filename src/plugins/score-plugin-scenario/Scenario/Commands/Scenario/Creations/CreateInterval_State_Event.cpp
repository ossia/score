// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CreateInterval_State_Event.hpp"

#include <Scenario/Commands/Scenario/Creations/CreateInterval_State.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/model/EntityMap.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/RandomNameProvider.hpp>

#include <QByteArray>

#include <vector>

namespace Scenario
{
namespace Command
{
CreateInterval_State_Event::CreateInterval_State_Event(
    const Scenario::ProcessModel& scenario,
    Id<StateModel> startState,
    Id<TimeSyncModel> endTimeSync,
    double endStateY,
    bool graph)
    : m_newEvent{getStrongId(scenario.events)}
    , m_createdName{RandomNameProvider::generateName<EventModel>()}
    , m_command{scenario, std::move(startState), m_newEvent, endStateY, graph}
    , m_endTimeSync{std::move(endTimeSync)}
{
}

void CreateInterval_State_Event::undo(const score::DocumentContext& ctx) const
{
  m_command.undo(ctx);

  ScenarioCreate<EventModel>::undo(
      m_newEvent, m_command.scenarioPath().find(ctx));
}

void CreateInterval_State_Event::redo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_command.scenarioPath().find(ctx);

  // Create the end event
  ScenarioCreate<EventModel>::redo(
      m_newEvent,
      scenar.timeSync(m_endTimeSync),
      scenar);

  scenar.events.at(m_newEvent).metadata().setName(m_createdName);

  // The state + interval between
  m_command.redo(ctx);
}

void CreateInterval_State_Event::serializeImpl(DataStreamInput& s) const
{
  s << m_newEvent << m_createdName << m_command.serialize() << m_endTimeSync;
}

void CreateInterval_State_Event::deserializeImpl(DataStreamOutput& s)
{
  QByteArray b;
  s >> m_newEvent >> m_createdName >> b >> m_endTimeSync;

  m_command.deserialize(b);
}
}
}
