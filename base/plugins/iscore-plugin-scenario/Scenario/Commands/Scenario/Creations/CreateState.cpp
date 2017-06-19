#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <algorithm>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <vector>

#include "CreateState.hpp"
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>

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
    , m_newState{getStrongId(scenario.states)}
    , m_event{std::move(event)}
    , m_stateY{stateY}
{
}

void CreateState::undo(const iscore::DocumentContext& ctx) const
{
  auto& scenar = m_path.find(ctx);

  ScenarioCreate<StateModel>::undo(m_newState, scenar);
  updateEventExtent(m_event, scenar);
}

void CreateState::redo(const iscore::DocumentContext& ctx) const
{
  auto& scenar = m_path.find(ctx);

  // Create the end state
  ScenarioCreate<StateModel>::redo(
      m_newState, scenar.events.at(m_event), m_stateY, scenar);

  updateEventExtent(m_event, scenar);
}

void CreateState::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_newState << m_event << m_stateY;
}

void CreateState::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_newState >> m_event >> m_stateY;
}
}
}
