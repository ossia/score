#pragma once

#include "MergeTimeSyncs.hpp"

#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Process/Algorithms/GoodOldDisplacementPolicy.hpp>
#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/AggregateCommand.hpp>
#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/Unused.hpp>

namespace Scenario
{

namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT MergeEvents final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), MergeEvents, "Merge events")
public:
  MergeEvents(const ProcessModel& scenario, Id<EventModel> clickedEv, Id<EventModel> hoveredEv);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;
  void update(unused_t, const Id<EventModel>&, const Id<EventModel>&);

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_scenarioPath;
  Id<EventModel> m_movingEventId;
  Id<EventModel> m_destinationEventId;

  QByteArray m_serializedEvent;
  MergeTimeSyncs* m_mergeTimeSyncsCommand{};
};

class MergeEventMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), MergeEventMacro, "Merge events")
};
}
}
