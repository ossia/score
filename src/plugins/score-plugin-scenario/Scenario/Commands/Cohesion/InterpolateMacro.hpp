#pragma once
#include <Scenario/Commands/Interval/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/AggregateCommand.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/detail/algorithms.hpp>

namespace Scenario
{
namespace Command
{
// RENAMEME
// One InterpolateMacro per interval
class AddMultipleProcessesToMultipleIntervalsMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      AddMultipleProcessesToMultipleIntervalsMacro,
      "Add processes to intervals")
};

class AddMultipleProcessesToIntervalMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      AddMultipleProcessesToIntervalMacro,
      "Add processes to interval")

public:
  auto& commands() { return m_cmds; }
  auto&& takeCommands() { return std::move(m_cmds); }

  // Use this constructor when the interval does not exist yet.
  AddMultipleProcessesToIntervalMacro(const Path<IntervalModel>& cstpath)
  {
    // Then create a slot in this rack
    auto cmd_slot = new Scenario::Command::AddSlotToRack{cstpath};
    addCommand(cmd_slot);

    slotsToUse.push_back({cstpath, 0});
  }

  // Use this constructor when the interval already exists
  AddMultipleProcessesToIntervalMacro(const IntervalModel& interval)
  {
    // If no slot : create slot
    if (interval.smallView().empty())
    {
      auto cmd_slot = new Scenario::Command::AddSlotToRack{interval};
      addCommand(cmd_slot);
    }

    // Put everything in first slot
    slotsToUse.push_back({interval, 0});
  }

  // No need to save this, it is useful only for construction.
  std::vector<SlotPath> slotsToUse;
};

inline AddMultipleProcessesToIntervalMacro*
makeAddProcessMacro(const IntervalModel& interval, int num_processes)
{
  return new AddMultipleProcessesToIntervalMacro{interval};
}
}
}
