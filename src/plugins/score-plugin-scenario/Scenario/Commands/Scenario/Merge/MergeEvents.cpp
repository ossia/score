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
{
  // Find the earliest.
  const auto& cev = scenario.events.at(clickedEv);
  const auto& hev = scenario.events.at(hoveredEv);
  if(&cev == &scenario.startEvent())
  {
    m_movingEventId = std::move(hoveredEv);
    m_destinationEventId = std::move(clickedEv);
  }
  else if(&hev == &scenario.startEvent())
  {
    m_movingEventId = std::move(clickedEv);
    m_destinationEventId = std::move(hoveredEv);
  }
  else if(cev.date() <= hev.date())
  {
    m_movingEventId = std::move(hoveredEv);
    m_destinationEventId = std::move(clickedEv);
  }
  else
  {
    m_movingEventId = std::move(clickedEv);
    m_destinationEventId = std::move(hoveredEv);
  }

  auto& mergedEvent = scenario.event(m_movingEventId);
  auto& destEvent = scenario.event(m_destinationEventId);

  QByteArray arr;
  DataStream::Serializer s{&arr};
  s.readFrom(mergedEvent);
  m_serializedEvent = arr;

  if(m_movingEventId != m_destinationEventId)
  {
    m_mergeTimeSyncsCommand = new MergeTimeSyncs{
        scenario, mergedEvent.timeSync(), destEvent.timeSync()};
  }
}

void MergeEvents::undo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_scenarioPath.find(ctx);

  auto& eventWhereThingsWereMoved = scenar.event(m_destinationEventId);

  DataStream::Deserializer s{m_serializedEvent};
  auto recreatedEvent = new EventModel{s, &scenar};

  const auto states_in_event = recreatedEvent->states();
  const auto old_ts = recreatedEvent->timeSync();

  // we remove and re-add states in recreated event
  // to ensure correct parentship between elements.
  for (auto stateId : states_in_event)
  {
    recreatedEvent->removeState(stateId);
    eventWhereThingsWereMoved.removeState(stateId);
  }

  recreatedEvent->changeTimeSync(eventWhereThingsWereMoved.timeSync());
  scenar.events.add(recreatedEvent);

  for (auto stateId : states_in_event)
  {
    recreatedEvent->addState(stateId);
    scenar.states.at(stateId).setEventId(m_movingEventId);
  }

  // Recreate the timesync
  auto& tn = scenar.timeSync(eventWhereThingsWereMoved.timeSync());
  if (m_mergeTimeSyncsCommand)
  {
    tn.addEvent(recreatedEvent->id());
    m_mergeTimeSyncsCommand->undo(ctx);
  }
  else
  {
    tn.addEvent(m_movingEventId);
  }
}

void MergeEvents::redo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_scenarioPath.find(ctx);
  auto& movingEvent = scenar.event(m_movingEventId);
  auto& destinationEvent = scenar.event(m_destinationEventId);

  auto movingStates = movingEvent.states();

  if (m_mergeTimeSyncsCommand)
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
    << m_serializedEvent << bool(m_mergeTimeSyncsCommand);
  if(m_mergeTimeSyncsCommand)
    s << m_mergeTimeSyncsCommand->serialize();
}

void MergeEvents::deserializeImpl(DataStreamOutput& s)
{
  bool hasCmd{};

  s >> m_scenarioPath >> m_movingEventId >> m_destinationEventId
      >> m_serializedEvent >> hasCmd;

  if(hasCmd)
  {
    QByteArray cmd;
    s >> cmd;

    m_mergeTimeSyncsCommand = new MergeTimeSyncs{};
    m_mergeTimeSyncsCommand->deserialize(cmd);
  }
}
}
}
