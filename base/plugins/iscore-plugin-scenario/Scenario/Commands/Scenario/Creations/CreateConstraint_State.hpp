#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/tools/std/Optional.hpp>

#include "CreateConstraint.hpp"
#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class ProcessModel;
class EventModel;
class StateModel;
class ConstraintModel;

namespace Command
{
class ISCORE_PLUGIN_SCENARIO_EXPORT CreateConstraint_State final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      CreateConstraint_State,
      "Create a constraint and a state")
public:
  CreateConstraint_State(
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

  const Id<ConstraintModel>& createdConstraint() const
  {
    return m_command.createdConstraint();
  }

  const Id<StateModel>& createdState() const
  {
    return m_newState;
  }

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Id<StateModel> m_newState;
  CreateConstraint m_command;
  Id<EventModel> m_endEvent;
  double m_stateY{};
};
}
}
