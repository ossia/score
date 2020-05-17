#pragma once

#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/GoodOldDisplacementPolicy.hpp>
#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <State/Expression.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/tools/Unused.hpp>
//#include <Scenario/Application/ScenarioValidity.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace Scenario
{

namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT MergeTimeSyncs final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), MergeTimeSyncs, "Synchronize")
public:
  MergeTimeSyncs(
      const ProcessModel& scenario,
      Id<TimeSyncModel> clickedTn,
      Id<TimeSyncModel> hoveredTn);
  ~MergeTimeSyncs() override;

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void
  update(unused_t scenar, const Id<TimeSyncModel>& clickedTn, const Id<TimeSyncModel>& hoveredTn);

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

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
