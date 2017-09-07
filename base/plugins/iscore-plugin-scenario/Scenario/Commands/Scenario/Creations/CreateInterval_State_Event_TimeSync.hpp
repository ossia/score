#pragma once
#include <Process/TimeValue.hpp>
#include <QString>
#include <iscore/command/Command.hpp>
#include <iscore/tools/std/Optional.hpp>

#include "CreateInterval_State_Event.hpp"
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_scenario_export.h>
struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class ProcessModel;
class IntervalModel;
class EventModel;
class StateModel;
class TimeSyncModel;
namespace Command
{
class ISCORE_PLUGIN_SCENARIO_EXPORT CreateInterval_State_Event_TimeSync final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      CreateInterval_State_Event_TimeSync,
      "Create a interval, a state, an event and a sync")
public:
  CreateInterval_State_Event_TimeSync(
      const Scenario::ProcessModel& scenario,
      Id<StateModel>
          startState,
      TimeVal date,
      double endStateY);

  const Path<Scenario::ProcessModel>& scenarioPath() const
  {
    return m_command.scenarioPath();
  }

  const Id<IntervalModel>& createdInterval() const
  {
    return m_command.createdInterval();
  }

  const Id<StateModel>& startState() const
  {
    return m_command.startState();
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
  QString m_createdName;

  CreateInterval_State_Event m_command;

  TimeVal m_date;
};
}
}
