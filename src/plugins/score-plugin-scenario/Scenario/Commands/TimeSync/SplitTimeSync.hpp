#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

#include <QVector>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class EventModel;
class TimeSyncModel;
namespace Command
{
class SplitTimeSync final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SplitTimeSync, "Desynchronize")
public:
  SplitTimeSync(const TimeSyncModel& path, QVector<Id<EventModel>> eventsInNewTimeSync);
  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<TimeSyncModel> m_path;
  QVector<Id<EventModel>> m_eventsInNewTimeSync;

  Id<TimeSyncModel> m_originalTimeSyncId;
  Id<TimeSyncModel> m_newTimeSyncId;
};

class SCORE_PLUGIN_SCENARIO_EXPORT SplitWholeSync final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SplitWholeSync, "Desynchronize")
public:
  SplitWholeSync(const TimeSyncModel& path);
  SplitWholeSync(const TimeSyncModel& path, std::vector<Id<TimeSyncModel>> new_ids);
  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<TimeSyncModel> m_path;

  Id<TimeSyncModel> m_originalTimeSync;
  std::vector<Id<TimeSyncModel>> m_newTimeSyncs;
};
}
}
