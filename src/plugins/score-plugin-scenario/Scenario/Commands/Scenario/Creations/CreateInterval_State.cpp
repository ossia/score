// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CreateInterval_State.hpp"

#include <Scenario/Commands/Scenario/Creations/CreateInterval.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/model/EntityMap.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/RandomNameProvider.hpp>

#include <QByteArray>

#include <vector>

namespace Scenario
{
namespace Command
{
CreateInterval_State::CreateInterval_State(
    const Scenario::ProcessModel& scenario,
    Id<StateModel> startState,
    Id<EventModel> endEvent,
    double endStateY,
    bool graphal)
    : m_createdName{RandomNameProvider::generateName<StateModel>()}
    , m_newState{getStrongId(scenario.states)}
    , m_command{scenario, std::move(startState), m_newState, graphal}
    , m_endEvent{std::move(endEvent)}
    , m_stateY{endStateY}
{
}

void CreateInterval_State::undo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_command.scenarioPath().find(ctx);
  m_command.undo(ctx);

  ScenarioCreate<StateModel>::undo(m_newState, scenar);
}

void CreateInterval_State::redo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_command.scenarioPath().find(ctx);
  auto& ev = scenar.events.at(m_endEvent);

  // Create the end state
  ScenarioCreate<StateModel>::redo(m_newState, ev, m_stateY, scenar);

  scenar.states.at(m_newState).metadata().setName(m_createdName);

  // The interval between
  m_command.redo(ctx);
}

void CreateInterval_State::serializeImpl(DataStreamInput& s) const
{
  s << m_newState << m_createdName << m_command.serialize() << m_endEvent << m_stateY;
}

void CreateInterval_State::deserializeImpl(DataStreamOutput& s)
{
  QByteArray b;
  s >> m_newState >> m_createdName >> b >> m_endEvent >> m_stateY;

  m_command.deserialize(b);
}
}
}
