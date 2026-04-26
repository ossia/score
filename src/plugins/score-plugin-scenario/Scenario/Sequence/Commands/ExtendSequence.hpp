#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <Process/TimeValue.hpp>

#include <score/command/AggregateCommand.hpp>
#include <score/model/Identifier.hpp>
#include <score/document/DocumentContext.hpp>

namespace Scenario
{
class ProcessModel;
class StateModel;
}

namespace Sequence
{
namespace Command
{

// Extends an existing sequence by appending a new section.
// Combines: MoveEventMeta (extends parent interval) + AppendSequenceSection.
class ExtendSequence final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(), ExtendSequence, "Extend sequence")
public:
  static ExtendSequence* make(
      const Scenario::ProcessModel& scenario,
      const Id<Scenario::StateModel>& endStateId,
      const TimeVal& newDate,
      double y);
};

}
}
