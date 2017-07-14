#pragma once

#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
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
class ISCORE_PLUGIN_SCENARIO_EXPORT MergeTimeNodes final
    : public iscore::Command
{
    ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), MergeTimeNodes, "Merge TimeNodes")
public:
  MergeTimeNodes(
      const ProcessModel& scenario,
      Id<TimeNodeModel>
          clickedTn,
      Id<TimeNodeModel>
          hoveredTn)
      : m_scenarioPath{scenario}
      , m_movingTnId{std::move(clickedTn)}
      , m_destinationTnId{std::move(hoveredTn)}
  {
    auto& tn = scenario.timeNode(m_movingTnId);
    auto& destinantionTn = scenario.timeNode(m_destinationTnId);

    QByteArray arr;
    DataStream::Serializer s{&arr};
    s.readFrom(tn);
    m_serializedTimeNode = arr;

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
    auto& globalTn = scenar.timeNode(m_destinationTnId);

    DataStream::Deserializer s{m_serializedTimeNode};
    auto recreatedTn = new TimeNodeModel{s, &scenar};

    auto events_in_timenode = recreatedTn->events();
    // we remove and re-add events in recreated Tn
    // to ensure correct parentship between elements.
    for (auto evId : events_in_timenode)
    {
      recreatedTn->removeEvent(evId);
      globalTn.removeEvent(evId);
    }
    for (auto evId : events_in_timenode)
    {
      recreatedTn->addEvent(evId);
    }

    scenar.timeNodes.add(recreatedTn);

    globalTn.setExpression(m_targetTrigger);
    globalTn.setActive(m_targetTriggerActive);

    //ScenarioValidityChecker::checkValidity(scenar);
    m_moveCommand->undo(ctx);
    updateTimeNodeExtent(m_destinationTnId, scenar);

    //ScenarioValidityChecker::checkValidity(scenar);
  }
  void redo(const iscore::DocumentContext& ctx) const override
  {
    auto& scenar = m_scenarioPath.find(ctx);
    //ScenarioValidityChecker::checkValidity(scenar);
    m_moveCommand->redo(ctx);
    //ScenarioValidityChecker::checkValidity(scenar);

    auto& movingTn = scenar.timeNode(m_movingTnId);
    auto& destinationTn = scenar.timeNode(m_destinationTnId);

    auto movingEvents = movingTn.events();
    for (auto& evId : movingEvents)
    {
      movingTn.removeEvent(evId);
      destinationTn.addEvent(evId);
    }
    destinationTn.setActive(
        destinationTn.active() || movingTn.active());
    destinationTn.setExpression(movingTn.expression());

    scenar.timeNodes.remove(m_movingTnId);
    updateTimeNodeExtent(m_destinationTnId, scenar);
    //ScenarioValidityChecker::checkValidity(scenar);
  }

  void update(
      unused_t scenar,
      const Id<TimeNodeModel>& clickedTn,
      const Id<TimeNodeModel>& hoveredTn)
  {
  }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_scenarioPath << m_movingTnId << m_destinationTnId
      << m_serializedTimeNode << m_moveCommand->serialize() << m_targetTrigger
      << m_targetTriggerActive;
  }

  void deserializeImpl(DataStreamOutput& s) override
  {
    QByteArray cmd;

    s >> m_scenarioPath >> m_movingTnId >> m_destinationTnId
        >> m_serializedTimeNode >> cmd >> m_targetTrigger
        >> m_targetTriggerActive;

    m_moveCommand = new MoveEvent<GoodOldDisplacementPolicy>{};
    m_moveCommand->deserialize(cmd);
  }

private:
  Path<ProcessModel> m_scenarioPath;
  Id<TimeNodeModel> m_movingTnId;
  Id<TimeNodeModel> m_destinationTnId;

  QByteArray m_serializedTimeNode;
  MoveEvent<GoodOldDisplacementPolicy>* m_moveCommand{};
  State::Expression m_targetTrigger;
  bool m_targetTriggerActive{};
};
}
}
