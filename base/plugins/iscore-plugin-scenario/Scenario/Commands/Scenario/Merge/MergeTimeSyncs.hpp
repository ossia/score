#pragma once

#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/GoodOldDisplacementPolicy.hpp>
#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <State/Expression.hpp>
#include <iscore/model/tree/TreeNode.hpp>

#include <iscore/command/Command.hpp>

#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/Identifier.hpp>
//#include <Scenario/Application/ScenarioValidity.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>

namespace Scenario
{

namespace Command
{
class ISCORE_PLUGIN_SCENARIO_EXPORT MergeTimeSyncs final
    : public iscore::Command
{
    ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), MergeTimeSyncs, "Merge TimeSyncs")
public:
  MergeTimeSyncs(
      const ProcessModel& scenario,
      Id<TimeSyncModel>
          clickedTn,
      Id<TimeSyncModel>
          hoveredTn)
      : m_scenarioPath{scenario}
      , m_movingTnId{std::move(clickedTn)}
      , m_destinationTnId{std::move(hoveredTn)}
  {
    auto& tn = scenario.timeSync(m_movingTnId);
    auto& destinantionTn = scenario.timeSync(m_destinationTnId);

    QByteArray arr;
    DataStream::Serializer s{&arr};
    s.readFrom(tn);
    m_serializedTimeSync = arr;

    m_moveCommand = new MoveEvent<GoodOldDisplacementPolicy>{
        scenario, tn.events().front(),
        destinantionTn.date(), ExpandMode::Scale};

    m_targetTrigger = destinantionTn.expression();
    m_targetTriggerActive = destinantionTn.active();
  }

  void undo(const iscore::DocumentContext& ctx) const override
  {
    auto& scenar = m_scenarioPath.find(ctx);

    //ScenarioValidityChecker::checkValidity(scenar);
    auto& globalTn = scenar.timeSync(m_destinationTnId);

    DataStream::Deserializer s{m_serializedTimeSync};
    auto recreatedTn = new TimeSyncModel{s, &scenar};

    auto events_in_timesync = recreatedTn->events();
    // we remove and re-add events in recreated Tn
    // to ensure correct parentship between elements.
    for (auto evId : events_in_timesync)
    {
      recreatedTn->removeEvent(evId);
      globalTn.removeEvent(evId);
    }
    for (auto evId : events_in_timesync)
    {
      recreatedTn->addEvent(evId);
    }

    scenar.timeSyncs.add(recreatedTn);

    globalTn.setExpression(m_targetTrigger);
    globalTn.setActive(m_targetTriggerActive);

    //ScenarioValidityChecker::checkValidity(scenar);
    m_moveCommand->undo(ctx);
    updateTimeSyncExtent(m_destinationTnId, scenar);

    //ScenarioValidityChecker::checkValidity(scenar);
  }
  void redo(const iscore::DocumentContext& ctx) const override
  {
    auto& scenar = m_scenarioPath.find(ctx);
    //ScenarioValidityChecker::checkValidity(scenar);
    m_moveCommand->redo(ctx);
    //ScenarioValidityChecker::checkValidity(scenar);

    auto& movingTn = scenar.timeSync(m_movingTnId);
    auto& destinationTn = scenar.timeSync(m_destinationTnId);

    auto movingEvents = movingTn.events();
    for (auto& evId : movingEvents)
    {
      movingTn.removeEvent(evId);
      destinationTn.addEvent(evId);
    }
    destinationTn.setActive(
        destinationTn.active() || movingTn.active());
    destinationTn.setExpression(movingTn.expression());

    scenar.timeSyncs.remove(m_movingTnId);
    updateTimeSyncExtent(m_destinationTnId, scenar);
    //ScenarioValidityChecker::checkValidity(scenar);
  }

  void update(
      unused_t scenar,
      const Id<TimeSyncModel>& clickedTn,
      const Id<TimeSyncModel>& hoveredTn)
  {
  }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_scenarioPath << m_movingTnId << m_destinationTnId
      << m_serializedTimeSync << m_moveCommand->serialize() << m_targetTrigger
      << m_targetTriggerActive;
  }

  void deserializeImpl(DataStreamOutput& s) override
  {
    QByteArray cmd;

    s >> m_scenarioPath >> m_movingTnId >> m_destinationTnId
        >> m_serializedTimeSync >> cmd >> m_targetTrigger
        >> m_targetTriggerActive;

    m_moveCommand = new MoveEvent<GoodOldDisplacementPolicy>{};
    m_moveCommand->deserialize(cmd);
  }

private:
  Path<ProcessModel> m_scenarioPath;
  Id<TimeSyncModel> m_movingTnId;
  Id<TimeSyncModel> m_destinationTnId;

  QByteArray m_serializedTimeSync;
  MoveEvent<GoodOldDisplacementPolicy>* m_moveCommand{};
  State::Expression m_targetTrigger;
  bool m_targetTriggerActive{};
};
}
}
