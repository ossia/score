#pragma once

#include "MergeTimeNodes.hpp"
#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Process/Algorithms/GoodOldDisplacementPolicy.hpp>
#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <iscore/command/Command.hpp>

#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/Identifier.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>

namespace Scenario
{

namespace Command
{
template <typename Scenario_T>
class ISCORE_PLUGIN_SCENARIO_EXPORT MergeEvents final : std::false_type
{
};

template <>
class ISCORE_PLUGIN_SCENARIO_EXPORT MergeEvents<ProcessModel> final
    : public iscore::Command
{
  // No ISCORE_COMMAND here since it's a template.
public:
  const CommandGroupKey& parentKey() const noexcept override
  {
    return ScenarioCommandFactoryName();
  }
  static const CommandKey& static_key() noexcept
  {
    auto name = QString("MergeEvents");
    static const CommandKey kagi{std::move(name)};
    return kagi;
  }
  const CommandKey& key() const noexcept override
  {
    return static_key();
  }
  QString description() const override
  {
    return QObject::tr("Merging Events");
  }

  MergeEvents() = default;

  MergeEvents(
      const ProcessModel& scenario,
      Id<EventModel>
          clickedEv,
      Id<EventModel>
          hoveredEv)
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

    m_mergeTimeNodesCommand = new MergeTimeNodes<ProcessModel>{
        scenario, event.timeNode(),
        destinantionEvent.timeNode()};
  }

  void undo(const iscore::DocumentContext& ctx) const override
  {
    auto& scenar = m_scenarioPath.find(ctx);
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

    if (recreatedEvent->timeNode() != globalEvent.timeNode())
    {
      auto& tn = scenar.timeNode(globalEvent.timeNode());
      tn.addEvent(m_movingEventId);
      m_mergeTimeNodesCommand->undo(ctx);
    }

    updateEventExtent(m_destinationEventId, scenar);
  }

  void redo(const iscore::DocumentContext& ctx) const override
  {
    auto& scenar = m_scenarioPath.find(ctx);
    auto& movingEvent = scenar.event(m_movingEventId);
    auto& destinationEvent = scenar.event(m_destinationEventId);
    auto movingStates = movingEvent.states();

    if (movingEvent.timeNode() != destinationEvent.timeNode())
      m_mergeTimeNodesCommand->redo(ctx);

    for (auto& stateId : movingStates)
    {
      movingEvent.removeState(stateId);
      destinationEvent.addState(stateId);
      scenar.states.at(stateId).setEventId(m_destinationEventId);
    }

    auto& tn = scenar.timeNode(destinationEvent.timeNode());
    tn.removeEvent(m_movingEventId);

    scenar.events.remove(m_movingEventId);
    updateEventExtent(m_destinationEventId, scenar);
  }

  void update(unused_t, const Id<EventModel>&, const Id<EventModel>&)
  {
  }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_scenarioPath << m_movingEventId << m_destinationEventId
      << m_serializedEvent << m_mergeTimeNodesCommand->serialize();
  }

  void deserializeImpl(DataStreamOutput& s) override
  {
    QByteArray cmd;

    s >> m_scenarioPath >> m_movingEventId >> m_destinationEventId
        >> m_serializedEvent >> cmd;

    m_mergeTimeNodesCommand = new MergeTimeNodes<ProcessModel>{};
    m_mergeTimeNodesCommand->deserialize(cmd);
  }

private:
  Path<ProcessModel> m_scenarioPath;
  Id<EventModel> m_movingEventId;
  Id<EventModel> m_destinationEventId;

  QByteArray m_serializedEvent;
  MergeTimeNodes<ProcessModel>* m_mergeTimeNodesCommand;
};
}
}

ISCORE_COMMAND_DECL_T(Scenario::Command::MergeEvents<Scenario::ProcessModel>)
