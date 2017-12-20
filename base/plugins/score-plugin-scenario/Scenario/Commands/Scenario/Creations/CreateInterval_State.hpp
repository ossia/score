#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <score/command/Command.hpp>
#include <score/tools/std/Optional.hpp>
#include <QString>

#include "CreateInterval.hpp"
#include <score/model/path/Path.hpp>
#include <score/model/Identifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class ProcessModel;
class EventModel;
class StateModel;
class IntervalModel;

namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT CreateInterval_State final
    : public score::Command
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      CreateInterval_State,
      "Create an interval and a state")
public:
  CreateInterval_State(
      const Scenario::ProcessModel& scenario,
      Id<StateModel> startState,
      Id<EventModel> endEvent,
      double endStateY);

  const Path<Scenario::ProcessModel>& scenarioPath() const
  {
    return m_command.scenarioPath();
  }

  const double& endStateY() const
  {
    return m_stateY;
  }

  const Id<StateModel>& startState() const
  {
    return m_command.startState();
  }

  const Id<IntervalModel>& createdInterval() const
  {
    return m_command.createdInterval();
  }

  const Id<StateModel>& createdState() const
  {
    return m_newState;
  }

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  QString m_createdName;
  Id<StateModel> m_newState;
  CreateInterval m_command;
  Id<EventModel> m_endEvent;
  double m_stateY{};
};
}
}
