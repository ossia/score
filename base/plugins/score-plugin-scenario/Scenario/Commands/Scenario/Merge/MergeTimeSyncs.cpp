#include "MergeTimeSyncs.hpp"
#include <Process/TimeValueSerialization.hpp>

namespace Scenario
{
namespace Command {

MergeTimeSyncs::MergeTimeSyncs(const ProcessModel& scenario, Id<TimeSyncModel> clickedTn, Id<TimeSyncModel> hoveredTn)
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
      destinantionTn.date(), ExpandMode::Scale, LockMode::Free};

  m_targetTrigger = destinantionTn.expression();
  m_targetTriggerActive = destinantionTn.active();
}

void MergeTimeSyncs::undo(const score::DocumentContext& ctx) const
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

void MergeTimeSyncs::redo(const score::DocumentContext& ctx) const
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

void MergeTimeSyncs::update(unused_t scenar, const Id<TimeSyncModel>& clickedTn, const Id<TimeSyncModel>& hoveredTn)
{
}

void MergeTimeSyncs::serializeImpl(DataStreamInput& s) const
{
  s << m_scenarioPath << m_movingTnId << m_destinationTnId
    << m_serializedTimeSync << m_moveCommand->serialize() << m_targetTrigger
    << m_targetTriggerActive;
}

void MergeTimeSyncs::deserializeImpl(DataStreamOutput& s)
{
  QByteArray cmd;

  s >> m_scenarioPath >> m_movingTnId >> m_destinationTnId
      >> m_serializedTimeSync >> cmd >> m_targetTrigger
      >> m_targetTriggerActive;

  m_moveCommand = new MoveEvent<GoodOldDisplacementPolicy>{};
  m_moveCommand->deserialize(cmd);
}


}
}
