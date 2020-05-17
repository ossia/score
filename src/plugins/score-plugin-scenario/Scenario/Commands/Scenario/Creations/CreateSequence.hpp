#pragma once
#include "CreateInterval_State_Event_TimeSync.hpp"

#include <Process/State/MessageNode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Cohesion/InterpolateMacro.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/command/CommandStackFacade.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class EventModel;
class IntervalModel;
class StateModel;
class TimeSyncModel;
class ProcessModel;
namespace Command
{

class CreateSequence final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), CreateSequence, "CreateSequence")

public:
  static CreateSequence* make(
      const score::DocumentContext& ctx,
      const Scenario::ProcessModel& scenario,
      const Id<StateModel>& start,
      const TimeVal& date,
      double endStateY);

  void undo(const score::DocumentContext& ctx) const override { m_cmds.front()->undo(ctx); }

  const Id<IntervalModel>& createdInterval() const { return m_newInterval; }

  const Id<StateModel>& createdState() const { return m_newState; }

  const Id<EventModel>& createdEvent() const { return m_newEvent; }

  const Id<TimeSyncModel>& createdTimeSync() const { return m_newTimeSync; }

private:
  Id<IntervalModel> m_newInterval;
  Id<StateModel> m_newState;
  Id<EventModel> m_newEvent;
  Id<TimeSyncModel> m_newTimeSync;
};

class CreateSequenceProcesses final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), CreateSequenceProcesses, "CreateSequenceData")

public:
  CreateSequenceProcesses(
      const Scenario::ProcessModel& scenario,
      const Scenario::IntervalModel& interval);

  int addedProcessCount() const { return m_addedProcessCount; }

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Scenario::ProcessModel> m_scenario;
  AddMultipleProcessesToIntervalMacro m_interpolations;
  Process::MessageNode m_stateData;
  Id<StateModel> m_endState;
  int m_addedProcessCount{};
};
}
}
