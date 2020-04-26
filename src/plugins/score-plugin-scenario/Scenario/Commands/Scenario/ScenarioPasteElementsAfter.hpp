#pragma once
#include <Process/Dataflow/Cable.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/selection/Selection.hpp>
#include <score/tools/std/Optional.hpp>

#include <rapidjson/document.h>
#include <QMap>
#include <QVector>
namespace Scenario
{
struct Point;
class EventModel;
class StateModel;
class TimeSyncModel;
class IntervalModel;
namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT ScenarioPasteElementsAfter final
    : public score::Command
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      ScenarioPasteElementsAfter,
      "Paste elements after sync")
public:
  ScenarioPasteElementsAfter(
      const Scenario::ProcessModel& path,
      const Scenario::TimeSyncModel& attach_sync,
      const rapidjson::Value& obj,
      double scale);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Scenario::ProcessModel> m_ts;
  Id<TimeSyncModel> m_attachSync;
  QVector<Id<EventModel>> m_eventsToAttach;

  // TODO std::vector...
  QVector<Id<TimeSyncModel>> m_ids_timesyncs;
  QVector<Id<IntervalModel>> m_ids_intervals;
  QVector<Id<EventModel>> m_ids_events;
  QVector<Id<StateModel>> m_ids_states;

  QVector<QByteArray> m_json_timesyncs;
  QVector<QByteArray> m_json_intervals;
  QVector<QByteArray> m_json_events;
  QVector<QByteArray> m_json_states;

  QMap<Id<Process::Cable>, Process::CableData> m_cables;
};
}
}
