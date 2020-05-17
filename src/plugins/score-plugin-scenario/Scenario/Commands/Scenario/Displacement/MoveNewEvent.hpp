#pragma once

#include "MoveEventOnCreationMeta.hpp"

#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/Unused.hpp>
#include <score/tools/std/Optional.hpp>

#include <score_plugin_scenario_export.h>

struct DataStreamInput;
struct DataStreamOutput;

/*
 * Used on creation mode, when mouse is pressed and is moving.
 * In this case, both vertical and horizontal move are allowed
 */

namespace Scenario
{
class EventModel;
class IntervalModel;
class ProcessModel;
namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT MoveNewEvent final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), MoveNewEvent, "Move a new event")
public:
  MoveNewEvent(
      const Scenario::ProcessModel& scenarioPath,
      Id<IntervalModel> intervalId,
      Id<EventModel> eventId,
      TimeVal date,
      const double y,
      bool yLocked);
  MoveNewEvent(
      const Scenario::ProcessModel& scenarioPath,
      Id<IntervalModel> intervalId,
      Id<EventModel> eventId,
      TimeVal date,
      const double y,
      bool yLocked,
      ExpandMode);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void update(
      Scenario::ProcessModel& s,
      unused_t,
      const Id<EventModel>& id,
      const TimeVal& date,
      const double y,
      bool yLocked)
  {
    m_cmd.update(s, id, date, y, ExpandMode::Scale, LockMode::Free);
    m_y = y;
    m_yLocked = yLocked;
  }

  const Path<Scenario::ProcessModel>& path() const { return m_path; }

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Scenario::ProcessModel> m_path;
  Id<IntervalModel> m_intervalId{};

  MoveEventOnCreationMeta m_cmd;
  double m_y{};
  bool m_yLocked{true}; // default is true and intervals are on the same y.
};
}
}
