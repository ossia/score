#include "RecordTools.hpp"

#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeNode_Event_State.hpp>
#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveNewEvent.hpp>
#include <Scenario/Commands/Constraint/AddRackToConstraint.hpp>
#include <Scenario/Commands/Constraint/Rack/AddSlotToRack.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
namespace Recording
{
Box CreateBox(
        Scenario::ScenarioModel &scenar,
        Scenario::Point pt,
        RecordCommandDispatcher &dispatcher)
{
    // Get the clicked point in scenario and create a state + constraint + state there
    // Create an automation + a rack + a slot + process views for all automations.
    auto default_end_date = pt.date;
    auto cmd_start = new Scenario::Command::CreateTimeNode_Event_State{
            scenar,
            pt.date,
            pt.y};
    cmd_start->redo();
    dispatcher.submitCommand(cmd_start);

    // TODO what happens if we go past the end of our scenario ? Stop recording ??
    auto cmd_end = new Scenario::Command::CreateConstraint_State_Event_TimeNode{
            scenar,
            cmd_start->createdState(),
            default_end_date,
            pt.y};
    cmd_end->redo();
    dispatcher.submitCommand(cmd_end);

    auto& cstr = scenar.constraints.at(cmd_end->createdConstraint());

    auto cmd_move = new Scenario::Command::MoveNewEvent(
                scenar,
                cstr.id(),
                cmd_end->createdEvent(),
                default_end_date,
                0,
                true,
                ExpandMode::Fixed);
    dispatcher.submitCommand(cmd_move);

    auto cmd_rack = new Scenario::Command::AddRackToConstraint{cstr};
    cmd_rack->redo();
    dispatcher.submitCommand(cmd_rack);
    auto& rack = cstr.racks.at(cmd_rack->createdRack());
    auto cmd_slot = new Scenario::Command::AddSlotToRack{rack};
    cmd_slot->redo();
    dispatcher.submitCommand(cmd_slot);

    for(const auto& vm : cstr.viewModels())
    {
        auto cmd_showrack = new Scenario::Command::ShowRackInViewModel{*vm, rack.id()};
        cmd_showrack->redo();
        dispatcher.submitCommand(cmd_showrack);
    }

    auto& slot = rack.slotmodels.at(cmd_slot->createdSlot());

    return {cstr, rack, slot, *cmd_move, cmd_end->createdEvent()};
}

}
