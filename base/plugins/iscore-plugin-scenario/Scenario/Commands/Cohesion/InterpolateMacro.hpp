#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <iscore/command/AggregateCommand.hpp>

#include <Scenario/Commands/Constraint/AddRackToConstraint.hpp>
#include <Scenario/Commands/Constraint/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Scenario/ShowRackInAllViewModels.hpp>
#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>

#include <ossia/detail/algorithms.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>

namespace Scenario
{
namespace Command
{
// RENAMEME
// One InterpolateMacro per constraint
class AddMultipleProcessesToMultipleConstraintsMacro final
    : public iscore::AggregateCommand
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      AddMultipleProcessesToMultipleConstraintsMacro,
      "Add processes to constraints")
};

class AddMultipleProcessesToConstraintMacro final
    : public iscore::AggregateCommand
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), AddMultipleProcessesToConstraintMacro,
      "Add processes to constraint")

public:
  auto& commands()
  {
    return m_cmds;
  }
  auto&& takeCommands()
  {
    return std::move(m_cmds);
  }

  // Use this constructor when the constraint does not exist yet.
  AddMultipleProcessesToConstraintMacro(const Path<ConstraintModel>& cstpath)
  {
    auto rackPath = cstpath.extend(ConstraintModel::smallViewRackId());

    // Then create a slot in this rack
    auto cmd_slot = new Scenario::Command::AddSlotToRack{
        Path<RackModel>{rackPath}};
    addCommand(cmd_slot);

    auto createdSlotPath = rackPath.extend(cmd_slot->createdSlot());
    slotsToUse.push_back(std::move(createdSlotPath));
  }

  // Use this constructor when the constraint already exists
  AddMultipleProcessesToConstraintMacro(const ConstraintModel& constraint)
  {
    // If no slot : create slot
    if(constraint.smallViewRack().slotmodels.empty())
    {
      auto cmd_slot = new Scenario::Command::AddSlotToRack{constraint.smallViewRack()};
      addCommand(cmd_slot);

      auto createdSlotPath = iscore::IDocument::path(constraint.smallViewRack())
                             .extend(cmd_slot->createdSlot());
      slotsToUse.push_back(std::move(createdSlotPath));
    }
    else
    {
      // Else put everything in first slot
      slotsToUse.push_back(*constraint.smallViewRack().slotmodels.begin());
    }
  }

  // No need to save this, it is useful only for construction.
  std::vector<Path<SlotModel>> slotsToUse;
};

inline AddMultipleProcessesToConstraintMacro*
    makeAddProcessMacro(const ConstraintModel& constraint, int num_processes)
{
  return new AddMultipleProcessesToConstraintMacro{
      constraint};
}
}
}
