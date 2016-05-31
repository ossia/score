#include "RecordTools.hpp"

#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeNode_Event_State.hpp>
#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveNewEvent.hpp>
#include <Scenario/Commands/Constraint/AddRackToConstraint.hpp>
#include <Scenario/Commands/Constraint/Rack/AddSlotToRack.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
namespace Recording
{

static QList<Device::Node*> GetParametersRecursive(Device::Node* parent)
{
    QList<Device::Node*> res;
    res.reserve(parent->childCount());
    for(auto& node : *parent)
    {
        res.push_back(&node);
        res.append(GetParametersRecursive(&node));
    }
    return res;
}

static QList<Device::Node*> GetParametersRecursive(QList<Device::Node*> parents)
{
    QList<Device::Node*> res;
    for(auto node : parents)
    {
        res.push_back(node);
        res.append(GetParametersRecursive(node));
    }

    auto end = res.end();
    for(auto it = res.begin(); it != end; )
    {
        bool ok = true;
        Device::Node* n = *it;
        ok &= n->is<Device::AddressSettings>();
        if(ok)
        {
            auto& as = n->get<Device::AddressSettings>();
            ok &= as.value.val.isValid()
               && (int)as.value.val.which() < (int)State::ValueType::NoValue;
        }

        if(ok)
        {
            ++it;
        }
        else
        {
            it = res.erase(it);
        }
    }

    return res;
}

std::vector<std::vector<Device::FullAddressSettings> >
    GetAddressesToRecordRecursive(Explorer::DeviceExplorerModel& explorer)
{
    std::vector<std::vector<Device::FullAddressSettings>> recordListening;

    auto parameters = GetParametersRecursive(
                  explorer.uniqueSelectedNodes(explorer.selectedIndexes()).parents);

    // First get the addresses to listen.
    for(auto node_ptr : parameters)
    {
        // TODO use address settings instead.
        auto& node = *node_ptr;
        if(!node.is<Device::AddressSettings>())
            continue;

        auto addr = Device::address(node);
        // TODO shall we check if the address is in, out, recordable ?
        // Recording an automation of strings would actually have a meaning
        // here (for instance recording someone typing).

        // We sort the addresses by device to optimize.
        auto dev_it = find_if(recordListening,
                              [&] (const auto& vec)
        { return vec.front().address.device == addr.device; });

        auto& as = node.get<Device::AddressSettings>();
        if(dev_it != recordListening.end())
        {
            dev_it->push_back(Device::FullAddressSettings::make<Device::FullAddressSettings::as_child>(as, addr));
        }
        else
        {
            recordListening.push_back({Device::FullAddressSettings::make<Device::FullAddressSettings::as_child>(as, addr)});
        }
    }

    return recordListening;
}

std::vector<std::vector<Device::FullAddressSettings> >
    GetAddressesToRecord(Explorer::DeviceExplorerModel& explorer)
{
    std::vector<std::vector<Device::FullAddressSettings>> recordListening;

    auto indices = explorer.selectedIndexes(); // TODO maybe filterUniqueParents and then recurse on the listening ??

    // First get the addresses to listen.
    for(auto& index : indices)
    {
        // TODO use address settings instead.
        auto& node = explorer.nodeFromModelIndex(index);
        if(!node.is<Device::AddressSettings>())
            continue;

        auto addr = Device::address(node);
        // TODO shall we check if the address is in, out, recordable ?
        // Recording an automation of strings would actually have a meaning
        // here (for instance recording someone typing).

        // We sort the addresses by device to optimize.
        auto dev_it = find_if(recordListening,
                              [&] (const auto& vec)
        { return vec.front().address.device == addr.device; });

        auto& as = node.get<Device::AddressSettings>();
        if(dev_it != recordListening.end())
        {
            dev_it->push_back(Device::FullAddressSettings::make<Device::FullAddressSettings::as_child>(as, addr));
        }
        else
        {
            recordListening.push_back({Device::FullAddressSettings::make<Device::FullAddressSettings::as_child>(as, addr)});
        }
    }

    return recordListening;
}

Box CreateBox(
        const Scenario::ScenarioModel &scenar,
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
