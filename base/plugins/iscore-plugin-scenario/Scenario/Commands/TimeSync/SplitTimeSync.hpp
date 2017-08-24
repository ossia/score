#pragma once

#include <QVector>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/model/Identifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class EventModel;
class TimeSyncModel;
namespace Command
{
class SplitTimeSync final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), SplitTimeSync, "Split a sync")
public:
  SplitTimeSync(
      const TimeSyncModel& path, QVector<Id<EventModel>> eventsInNewTimeSync);
  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<TimeSyncModel> m_path;
  QVector<Id<EventModel>> m_eventsInNewTimeSync;

  Id<TimeSyncModel> m_originalTimeSyncId;
  Id<TimeSyncModel> m_newTimeSyncId;
};
}
}
