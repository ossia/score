#pragma once
#include "CreateEvent_State.hpp"

#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

#include <QString>

#include <score_plugin_scenario_export.h>
struct DataStreamInput;
struct DataStreamOutput;
namespace Scenario
{
class ProcessModel;
class EventModel;
class StateModel;
class TimeSyncModel;
namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT CreateTimeSync_Event_State final : public score::Command
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      CreateTimeSync_Event_State,
      "Create a timesync, an event and a state")
public:
  CreateTimeSync_Event_State(const Scenario::ProcessModel& scenario, TimeVal date, double stateY);

  const Path<Scenario::ProcessModel>& scenarioPath() const { return m_command.scenarioPath(); }

  const Id<StateModel>& createdState() const { return m_command.createdState(); }

  const Id<EventModel>& createdEvent() const { return m_command.createdEvent(); }

  const Id<TimeSyncModel>& createdTimeSync() const { return m_newTimeSync; }

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Id<TimeSyncModel> m_newTimeSync;
  QString m_createdName;
  TimeVal m_date;

  CreateEvent_State m_command;
};
}
}
