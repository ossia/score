#pragma once
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <Process/Dataflow/Cable.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/selection/Selection.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/json.hpp>

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
  std::vector<Id<EventModel>> m_eventsToAttach;

  // TODO std::vector...
  std::vector<Id<TimeSyncModel>> m_ids_timesyncs;
  std::vector<Id<IntervalModel>> m_ids_intervals;
  std::vector<Id<EventModel>> m_ids_events;
  std::vector<Id<StateModel>> m_ids_states;

  std::vector<QByteArray> m_json_timesyncs;
  std::vector<QByteArray> m_json_intervals;
  std::vector<QByteArray> m_json_events;
  std::vector<QByteArray> m_json_states;

  CopiedCables m_cables;
};
}
}
