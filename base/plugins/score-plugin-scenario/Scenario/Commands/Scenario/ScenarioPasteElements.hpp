#pragma once
#include <QJsonObject>
#include <QMap>
#include <QVector>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <score/command/Command.hpp>
#include <score/tools/std/Optional.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/Identifier.hpp>
#include <score/selection/Selection.hpp>

namespace Scenario
{
struct Point;
class EventModel;
class StateModel;
class TimeSyncModel;
class IntervalModel;
namespace Command
{

class SCORE_PLUGIN_SCENARIO_EXPORT ScenarioPasteElements final : public score::Command
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      ScenarioPasteElements,
      "Paste elements in scenario")
public:
  ScenarioPasteElements(
      const Scenario::ProcessModel& path,
      const QJsonObject& obj,
      const Scenario::Point& pt);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Scenario::ProcessModel> m_ts;

  // TODO std::vector...
  QVector<Id<TimeSyncModel>> m_ids_timesyncs;
  QVector<Id<IntervalModel>> m_ids_intervals;
  QVector<Id<EventModel>> m_ids_events;
  QVector<Id<StateModel>> m_ids_states;

  QVector<QJsonObject> m_json_timesyncs;
  QVector<QJsonObject> m_json_intervals;
  QVector<QJsonObject> m_json_events;
  QVector<QJsonObject> m_json_states;
};


class SCORE_PLUGIN_SCENARIO_EXPORT ScenarioPasteElementsAfter final : public score::Command
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      ScenarioPasteElementsAfter,
      "Paste elements after sync")
public:
  ScenarioPasteElementsAfter(
      const Scenario::ProcessModel& path,
      const QJsonObject& obj);

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

  QVector<QJsonObject> m_json_timesyncs;
  QVector<QJsonObject> m_json_intervals;
  QVector<QJsonObject> m_json_events;
  QVector<QJsonObject> m_json_states;
};
}
}
