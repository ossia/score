#include "MergeEvents.hpp"
#include <score/model/EntitySerialization.hpp>

namespace Scenario
{
namespace Command
{

MergeEvents::MergeEvents(
    const ProcessModel& scenario,
    Id<EventModel> clickedEv,
    Id<EventModel> hoveredEv)
    : m_scenarioPath{scenario}
    , m_movingEventId{std::move(clickedEv)}
    , m_destinationEventId{std::move(hoveredEv)}
{
  auto& event = scenario.event(m_movingEventId);
  auto& destinantionEvent = scenario.event(m_destinationEventId);

  QByteArray arr;
  DataStream::Serializer s{&arr};
  s.readFrom(event);
  m_serializedEvent = arr;

  m_mergeTimeSyncsCommand = new MergeTimeSyncs{
      scenario, event.timeSync(), destinantionEvent.timeSync()};
}

void MergeEvents::undo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_scenarioPath.find(ctx);

  // ScenarioValidityChecker::checkValidity(scenar);
  auto& globalEvent = scenar.event(m_destinationEventId);

  DataStream::Deserializer s{m_serializedEvent};
  auto recreatedEvent = new EventModel{s, &scenar};

  auto states_in_event = recreatedEvent->states();
  // we remove and re-add states in recreated event
  // to ensure correct parentship between elements.
  for (auto stateId : states_in_event)
  {
    recreatedEvent->removeState(stateId);
    globalEvent.removeState(stateId);
  }
  for (auto stateId : states_in_event)
  {
    recreatedEvent->addState(stateId);
    scenar.states.at(stateId).setEventId(m_movingEventId);
  }

  scenar.events.add(recreatedEvent);

  auto& tn = scenar.timeSync(globalEvent.timeSync());
  if (recreatedEvent->timeSync() != globalEvent.timeSync())
  {
    tn.addEvent(m_movingEventId);
    // ScenarioValidityChecker::checkValidity(scenar);
    m_mergeTimeSyncsCommand->undo(ctx);
    // ScenarioValidityChecker::checkValidity(scenar);
  }
  else
  {
    // recreatedEvent->timeSync == globalEvent->timeSync:
    // both events originally were on the same time sync.
    // auto it = ossia::find(tn.events(), m_movingEventId);
    // SCORE_ASSERT(it == tn.events().end());
    tn.addEvent(m_movingEventId);
    // ScenarioValidityChecker::checkValidity(scenar);
  }

  // ScenarioValidityChecker::checkValidity(scenar);
}

void MergeEvents::redo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_scenarioPath.find(ctx);
  // ScenarioValidityChecker::checkValidity(scenar);
  auto& movingEvent = scenar.event(m_movingEventId);
  auto& destinationEvent = scenar.event(m_destinationEventId);
  auto movingStates = movingEvent.states();

  if (movingEvent.timeSync() != destinationEvent.timeSync())
    m_mergeTimeSyncsCommand->redo(ctx);

  for (auto& stateId : movingStates)
  {
    movingEvent.removeState(stateId);
    destinationEvent.addState(stateId);
    scenar.states.at(stateId).setEventId(m_destinationEventId);
  }

  auto& ts = scenar.timeSync(destinationEvent.timeSync());
  ts.removeEvent(m_movingEventId);

  scenar.events.remove(m_movingEventId);
  // ScenarioValidityChecker::checkValidity(scenar);
}

void MergeEvents::update(
    unused_t,
    const Id<EventModel>&,
    const Id<EventModel>&)
{
}

void MergeEvents::serializeImpl(DataStreamInput& s) const
{
  s << m_scenarioPath << m_movingEventId << m_destinationEventId
    << m_serializedEvent << m_mergeTimeSyncsCommand->serialize();
}

void MergeEvents::deserializeImpl(DataStreamOutput& s)
{
  QByteArray cmd;

  s >> m_scenarioPath >> m_movingEventId >> m_destinationEventId
      >> m_serializedEvent >> cmd;

  m_mergeTimeSyncsCommand = new MergeTimeSyncs{};
  m_mergeTimeSyncsCommand->deserialize(cmd);
}
}
}
