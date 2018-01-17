#pragma once
#include <score/command/AggregateCommand.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Interval/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Interval/Rack/Slot/AddLayerModelToSlot.hpp>

#include <Process/ProcessFactory.hpp>


namespace Scenario
{
class IntervalModel;
namespace Command
{
class AddProcessInNewSlot final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), AddProcessInNewSlot,
      "Create a process in a new slot")

  public:
  template<typename Dispatcher>
  static void create(
      Dispatcher & disp,
      const Scenario::IntervalModel& interval,
      UuidKey<Process::ProcessModel> proc,
      const QString& dat)
  {
    auto cmd1 = new Scenario::Command::AddOnlyProcessToInterval(
        interval, proc, dat);
    cmd1->redo(disp.stack().context());
    disp.submitCommand(cmd1);

    auto cmd2 = new Scenario::Command::AddSlotToRack(interval);
    cmd2->redo(disp.stack().context());
    disp.submitCommand(cmd2);

    auto cmd3 = new Scenario::Command::AddLayerModelToSlot(
        SlotPath{interval, int(interval.smallView().size() - 1)}, cmd1->processId());
    cmd3->redo(disp.stack().context());
    disp.submitCommand(cmd3);

  }
};

class DuplicateProcess final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), DuplicateProcess,
      "Duplicate a process")

  public:
  template<typename Dispatcher>
  static void create(
      Dispatcher & disp,
      const Scenario::IntervalModel& interval,
      const Process::ProcessModel& proc)
  {
    auto cmd1 = new Scenario::Command::DuplicateOnlyProcessToInterval(
        interval, proc);
    cmd1->redo(disp.stack().context());
    disp.submitCommand(cmd1);

    auto cmd2 = new Scenario::Command::AddSlotToRack(interval);
    cmd2->redo(disp.stack().context());
    disp.submitCommand(cmd2);

    auto cmd3 = new Scenario::Command::AddLayerModelToSlot(
        SlotPath{interval, int(interval.smallView().size() - 1)}, cmd1->processId());
    cmd3->redo(disp.stack().context());
    disp.submitCommand(cmd3);

  }
};
}
}
