#pragma once
#include <QString>
#include <iscore/command/Command.hpp>
#include <iscore/tools/std/Optional.hpp>

#include "CreateState.hpp"
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class EventModel;
class StateModel;
class ProcessModel;
class TimeSyncModel;
namespace Command
{
class ISCORE_PLUGIN_SCENARIO_EXPORT CreateEvent_State final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      CreateEvent_State,
      "Create an event and a state")
public:
  CreateEvent_State(
      const Scenario::ProcessModel& scenario,
      Id<TimeSyncModel>
          timeSync,
      double stateY);

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
    return m_newEvent;
  }

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Id<EventModel> m_newEvent;
  QString m_createdName;

  CreateState m_command;

  Id<TimeSyncModel> m_timeSync;
};
}
}
