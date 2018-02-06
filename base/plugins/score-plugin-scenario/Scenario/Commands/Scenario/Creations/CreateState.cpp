// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <score/tools/RandomNameProvider.hpp>

#include <algorithm>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <vector>

#include "CreateState.hpp"
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
CreateState::CreateState(
    const Scenario::ProcessModel& scenario,
    Id<EventModel>
        event,
    double stateY)
    : m_path{scenario}
    , m_createdName{RandomNameProvider::generateName<StateModel>()}
    , m_newState{getStrongId(scenario.states)}
    , m_event{std::move(event)}
    , m_stateY{stateY}
{
}
CreateState::CreateState(
    const Scenario::ProcessModel& scenario,
    Id<StateModel> newId,
    Id<EventModel> event,
    double stateY)
  : m_path{scenario}
  , m_createdName{RandomNameProvider::generateName<StateModel>()}
  , m_newState{newId}
  , m_event{std::move(event)}
  , m_stateY{stateY}
{
}

void CreateState::undo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_path.find(ctx);

  ScenarioCreate<StateModel>::undo(m_newState, scenar);
  updateEventExtent(m_event, scenar);
}

void CreateState::redo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_path.find(ctx);

  // Create the end state
  ScenarioCreate<StateModel>::redo(
      m_newState, scenar.events.at(m_event), m_stateY, scenar);

  scenar.states.at(m_newState).metadata().setName(m_createdName);

  updateEventExtent(m_event, scenar);
}

void CreateState::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_newState << m_createdName <<  m_event << m_stateY;
}

void CreateState::deserializeImpl(DataStreamOutput& s)
{

  s >> m_path >> m_newState >> m_createdName >> m_event >> m_stateY;

}
}
}
