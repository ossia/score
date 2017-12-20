// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <score/tools/RandomNameProvider.hpp>

#include <QByteArray>
#include <algorithm>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <vector>

#include "CreateInterval_State.hpp"
#include <Scenario/Commands/Scenario/Creations/CreateInterval.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/EntityMap.hpp>

namespace Scenario
{
namespace Command
{
CreateInterval_State::CreateInterval_State(
    const Scenario::ProcessModel& scenario,
    Id<StateModel> startState,
    Id<EventModel> endEvent,
    double endStateY)
    : m_createdName{RandomNameProvider::generateName<StateModel>()}
    , m_newState{getStrongId(scenario.states)}
    , m_command{scenario, std::move(startState), m_newState}
    , m_endEvent{std::move(endEvent)}
    , m_stateY{endStateY}
{
}

void CreateInterval_State::undo(const score::DocumentContext& ctx) const
{
  m_command.undo(ctx);

  ScenarioCreate<StateModel>::undo(
      m_newState, m_command.scenarioPath().find(ctx));
  updateEventExtent(m_endEvent, m_command.scenarioPath().find(ctx));
}

void CreateInterval_State::redo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_command.scenarioPath().find(ctx);

  // Create the end state
  ScenarioCreate<StateModel>::redo(
      m_newState, scenar.events.at(m_endEvent), m_stateY, scenar);

    scenar.states.at(m_newState).metadata().setName(m_createdName);

  // The interval between
  m_command.redo(ctx);
  updateEventExtent(m_endEvent, scenar);
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
