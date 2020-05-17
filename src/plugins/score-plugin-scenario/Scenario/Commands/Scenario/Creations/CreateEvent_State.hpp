#pragma once
#include "CreateState.hpp"

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

#include <QString>

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
class SCORE_PLUGIN_SCENARIO_EXPORT CreateEvent_State final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), CreateEvent_State, "Create an event and a state")
public:
  CreateEvent_State(
      const Scenario::ProcessModel& scenario,
      Id<TimeSyncModel> timeSync,
      double stateY);

  const Path<Scenario::ProcessModel>& scenarioPath() const { return m_command.scenarioPath(); }

  const Id<StateModel>& createdState() const { return m_command.createdState(); }

  const Id<EventModel>& createdEvent() const { return m_newEvent; }

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

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
