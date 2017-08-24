#pragma once
#include <Process/TimeValue.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/tools/std/Optional.hpp>

#include "CreateEvent_State.hpp"
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_scenario_export.h>
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
class ISCORE_PLUGIN_SCENARIO_EXPORT CreateTimeSync_Event_State final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      CreateTimeSync_Event_State,
      "Create a timesync, an event and a state")
public:
  CreateTimeSync_Event_State(
      const Scenario::ProcessModel& scenario, TimeVal date, double stateY);

  const Path<Scenario::ProcessModel>& scenarioPath() const
  {
    return m_command.scenarioPath();
  }

  const Id<StateModel>& createdState() const
  {
    return m_command.createdState();
  }

  const Id<EventModel>& createdEvent() const
  {
    return m_command.createdEvent();
  }

  const Id<TimeSyncModel>& createdTimeSync() const
  {
    return m_newTimeSync;
  }

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Id<TimeSyncModel> m_newTimeSync;
  TimeVal m_date;

  CreateEvent_State m_command;
};
}
}
