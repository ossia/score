#pragma once
#include <Process/State/MessageNode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Cohesion/InterpolateMacro.hpp>
#include <iscore/command/CommandStackFacade.hpp>
#include <iscore/command/Command.hpp>

#include "CreateConstraint_State_Event_TimeNode.hpp"
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class EventModel;
class ConstraintModel;
class StateModel;
class TimeNodeModel;
class ProcessModel;
namespace Command
{

class CreateSequence final : public iscore::AggregateCommand
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), CreateSequence, "CreateSequence")

public:
  static CreateSequence* make(
      const Scenario::ProcessModel& scenario,
      const Id<StateModel>& start,
      const TimeValue& date,
      double endStateY);

  void undo() const override
  {
    m_cmds.front()->undo();
  }

  const Id<ConstraintModel>& createdConstraint() const
  {
    return m_newConstraint;
  }

  const Id<StateModel>& createdState() const
  {
    return m_newState;
  }

  const Id<EventModel>& createdEvent() const
  {
    return m_newEvent;
  }

  const Id<TimeNodeModel>& createdTimeNode() const
  {
    return m_newTimeNode;
  }

private:
  Id<ConstraintModel> m_newConstraint;
  Id<StateModel> m_newState;
  Id<EventModel> m_newEvent;
  Id<TimeNodeModel> m_newTimeNode;
};

class CreateSequenceProcesses final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      CreateSequenceProcesses,
      "CreateSequenceData")

public:
  CreateSequenceProcesses(
      const Scenario::ProcessModel& scenario,
      const Scenario::ConstraintModel& constraint);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Scenario::ProcessModel> m_scenario;
  AddMultipleProcessesToConstraintMacro m_interpolations;
  Process::MessageNode m_stateData;
  Id<StateModel> m_endState;
  int m_addedProcessCount{};
};
}
}
