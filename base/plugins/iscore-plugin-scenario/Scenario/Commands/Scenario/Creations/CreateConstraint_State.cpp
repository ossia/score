// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <QByteArray>
#include <algorithm>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <vector>

#include "CreateConstraint_State.hpp"
#include <Scenario/Commands/Scenario/Creations/CreateConstraint.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>

namespace Scenario
{
namespace Command
{
CreateConstraint_State::CreateConstraint_State(
    const Scenario::ProcessModel& scenario,
    Id<StateModel> startState,
    Id<EventModel> endEvent,
    double endStateY)
    : m_newState{getStrongId(scenario.states)}
    , m_command{scenario, std::move(startState), m_newState}
    , m_endEvent{std::move(endEvent)}
    , m_stateY{endStateY}
{
}

void CreateConstraint_State::undo(const iscore::DocumentContext& ctx) const
{
  m_command.undo(ctx);

  ScenarioCreate<StateModel>::undo(
      m_newState, m_command.scenarioPath().find(ctx));
  updateEventExtent(m_endEvent, m_command.scenarioPath().find(ctx));
}

void CreateConstraint_State::redo(const iscore::DocumentContext& ctx) const
{
  auto& scenar = m_command.scenarioPath().find(ctx);

  // Create the end state
  ScenarioCreate<StateModel>::redo(
      m_newState, scenar.events.at(m_endEvent), m_stateY, scenar);

  // The constraint between
  m_command.redo(ctx);
  updateEventExtent(m_endEvent, scenar);
}

void CreateConstraint_State::serializeImpl(DataStreamInput& s) const
{
  s << m_newState << m_command.serialize() << m_endEvent << m_stateY;
}

void CreateConstraint_State::deserializeImpl(DataStreamOutput& s)
{
  QByteArray b;
  s >> m_newState >> b >> m_endEvent >> m_stateY;

  m_command.deserialize(b);
}
}
}
