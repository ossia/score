// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CreateEvent_State.hpp"

#include <Scenario/Commands/Scenario/Creations/CreateState.hpp>
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
CreateEvent_State::CreateEvent_State(
    const Scenario::ProcessModel& scenario,
    Id<TimeSyncModel> timeSync,
    double stateY)
    : m_newEvent{getStrongId(scenario.events)}
    , m_createdName{RandomNameProvider::generateName<EventModel>()}
    , m_command{scenario, m_newEvent, stateY}
    , m_timeSync{std::move(timeSync)}
{
}

void CreateEvent_State::undo(const score::DocumentContext& ctx) const
{
  m_command.undo(ctx);

  ScenarioCreate<EventModel>::undo(m_newEvent, m_command.scenarioPath().find(ctx));
}

void CreateEvent_State::redo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_command.scenarioPath().find(ctx);

  // Create the event
  ScenarioCreate<EventModel>::redo(m_newEvent, scenar.timeSync(m_timeSync), scenar);

  scenar.events.at(m_newEvent).metadata().setName(m_createdName);
  // scenar.events.at(m_newEvent).setCondition(State::defaultFalseExpression());

  // And the state
  m_command.redo(ctx);
}

void CreateEvent_State::serializeImpl(DataStreamInput& s) const
{
  s << m_newEvent << m_createdName << m_command.serialize() << m_timeSync;
}

void CreateEvent_State::deserializeImpl(DataStreamOutput& s)
{
  QByteArray b;
  s >> m_newEvent >> m_createdName >> b >> m_timeSync;

  m_command.deserialize(b);
}
}
}
