#pragma once
#include <iscore/command/AggregateCommand.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Slot.hpp>
#include <Scenario/Commands/Constraint/AddOnlyProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>

#include <Process/ProcessFactory.hpp>


namespace Scenario
{
class ConstraintModel;
namespace Command
{
class CreateProcessInNewSlot final : public iscore::AggregateCommand
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), CreateProcessInNewSlot,
      "Create a process in a new slot")

  public:
  template<typename Dispatcher>
  static void create(
      Dispatcher & disp,
      const Scenario::ConstraintModel& constraint,
      UuidKey<Process::ProcessModel> proc)
  {
    auto cmd1 = new Scenario::Command::AddOnlyProcessToConstraint(
        constraint, proc);
    cmd1->redo(disp.stack().context());
    disp.submitCommand(cmd1);

    auto cmd2 = new Scenario::Command::AddSlotToRack(constraint);
    cmd2->redo(disp.stack().context());
    disp.submitCommand(cmd2);

    auto cmd3 = new Scenario::Command::AddLayerModelToSlot(
        SlotPath{constraint, int(constraint.smallView().size() - 1)}, cmd1->processId());
    cmd3->redo(disp.stack().context());
    disp.submitCommand(cmd3);

  }
};
}
}
