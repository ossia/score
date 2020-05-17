#pragma once
#include "CreateInterval_State.hpp"

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
class ProcessModel;
class EventModel;
class StateModel;
class TimeSyncModel;
class IntervalModel;
namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT CreateInterval_State_Event final : public score::Command
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      CreateInterval_State_Event,
      "Create an interval, a state and an event")
public:
  CreateInterval_State_Event(
      const Scenario::ProcessModel& scenario,
      Id<StateModel> startState,
      Id<TimeSyncModel> endTimeSync,
      double endStateY,
      bool graphal);

  const Path<Scenario::ProcessModel>& scenarioPath() const { return m_command.scenarioPath(); }

  const double& endStateY() const { return m_command.endStateY(); }

  const Id<IntervalModel>& createdInterval() const { return m_command.createdInterval(); }

  const Id<StateModel>& startState() const { return m_command.startState(); }

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

  CreateInterval_State m_command;

  Id<TimeSyncModel> m_endTimeSync;
};
}
}
