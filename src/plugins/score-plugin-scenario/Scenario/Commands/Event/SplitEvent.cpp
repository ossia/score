// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SplitEvent.hpp"

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/model/EntityMap.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/RandomNameProvider.hpp>

#include <vector>

namespace Scenario
{
namespace Command
{
SplitEvent::SplitEvent(
    const Scenario::ProcessModel& scenario,
    Id<EventModel> event,
    QVector<Id<StateModel>> movingstates)
    : m_scenarioPath{scenario}
    , m_originalEvent{std::move(event)}
    , m_newEvent{getStrongId(scenario.events)}
    , m_createdName{RandomNameProvider::generateName<EventModel>()}
    , m_movingStates{std::move(movingstates)}
{
}

SplitEvent::SplitEvent(
    const Scenario::ProcessModel& scenario,
    Id<EventModel> event,
    Id<EventModel> new_event,
    QVector<Id<StateModel>> movingstates)
    : m_scenarioPath{scenario}
    , m_originalEvent{std::move(event)}
    , m_newEvent{new_event}
    , m_createdName{RandomNameProvider::generateName<EventModel>()}
    , m_movingStates{std::move(movingstates)}
{
}
void SplitEvent::undo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_scenarioPath.find(ctx);
  auto& originalEvent = scenar.events.at(m_originalEvent);

  for (auto& st : m_movingStates)
  {
    originalEvent.addState(st);
    scenar.states.at(st).setEventId(m_originalEvent);
  }

  ScenarioCreate<EventModel>::undo(m_newEvent, m_scenarioPath.find(ctx));
}

void SplitEvent::redo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_scenarioPath.find(ctx);
  auto& originalEvent = scenar.event(m_originalEvent);
  ScenarioCreate<EventModel>::redo(
      m_newEvent, scenar.timeSyncs.at(originalEvent.timeSync()), scenar);

  auto& newEvent = scenar.events.at(m_newEvent);
  newEvent.metadata().setName(m_createdName);

  newEvent.setCondition(originalEvent.condition());

  for (auto& st : m_movingStates)
  {
    originalEvent.removeState(st);
    newEvent.addState(st);
    scenar.states.at(st).setEventId(m_newEvent);
  }
}

void SplitEvent::serializeImpl(DataStreamInput& s) const
{
  s << m_scenarioPath << m_originalEvent << m_newEvent << m_createdName << m_movingStates;
}

void SplitEvent::deserializeImpl(DataStreamOutput& s)
{
  s >> m_scenarioPath >> m_originalEvent >> m_newEvent >> m_createdName >> m_movingStates;
}

SplitWholeEvent::SplitWholeEvent(const EventModel& path) : m_path{path}
{
  SCORE_ASSERT(path.states().size() > 1);
  m_originalEvent = path.id();
  auto scenar = static_cast<Scenario::ProcessModel*>(path.parent());
  m_newEvents = getStrongIdRange<Scenario::EventModel>(path.states().size() - 1, scenar->events);
}

void SplitWholeEvent::undo(const score::DocumentContext& ctx) const
{
  auto& scenar = static_cast<Scenario::ProcessModel&>(*m_path.find(ctx).parent());

  auto& originalEvent = scenar.event(m_originalEvent);
  for (const auto& id : m_newEvents)
  {
    auto& newEvent = scenar.event(id);

    SCORE_ASSERT(newEvent.states().size() == 1);
    const auto& stateId = newEvent.states().front();
    {
      auto& st = scenar.state(stateId);
      newEvent.removeState(stateId);
      originalEvent.addState(stateId);
      st.setEventId(m_originalEvent);
    }

    ScenarioCreate<EventModel>::undo(id, scenar);
  }
}

void SplitWholeEvent::redo(const score::DocumentContext& ctx) const
{
  auto& scenar = static_cast<Scenario::ProcessModel&>(*m_path.find(ctx).parent());
  auto& originalEvent = scenar.event(m_originalEvent);
  auto& originalTS = scenar.timeSyncs.at(originalEvent.timeSync());
  auto originalStates = originalEvent.states();

  std::size_t k = 1;
  for (const auto& id : m_newEvents)
  {
    // TODO set the correct position here.
    EventModel& ev = ScenarioCreate<EventModel>::redo(id, originalTS, scenar);

    ev.setCondition(originalEvent.condition());

    const auto& stateId = originalStates[k];
    auto& st = scenar.state(stateId);
    originalEvent.removeState(stateId);

    ev.addState(stateId);

    st.setEventId(id);
    k++;
  }
}

void SplitWholeEvent::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_originalEvent << m_newEvents;
}

void SplitWholeEvent::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_originalEvent >> m_newEvents;
}
}
}
