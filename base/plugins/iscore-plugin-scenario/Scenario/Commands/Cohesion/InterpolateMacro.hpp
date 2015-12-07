#pragma once
#include <iscore/command/AggregateCommand.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>

#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>
#include <Scenario/Commands/Scenario/ShowRackInAllViewModels.hpp>
#include <Scenario/Commands/Constraint/AddRackToConstraint.hpp>
#include <Scenario/Commands/Constraint/Rack/AddSlotToRack.hpp>

// RENAMEME
// One InterpolateMacro per constraint
class AddMultipleProcessesToMultipleConstraintsMacro final : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), AddMultipleProcessesToMultipleConstraintsMacro, "Add processes to constraints")
};

class AddMultipleProcessesToConstraintMacro final : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), AddMultipleProcessesToConstraintMacro, "Add processes to constraint")

        public:
            auto& commands() { return m_cmds; }

        // Use this constructor when the constraint does not exist yet.
        AddMultipleProcessesToConstraintMacro(const Path<ConstraintModel>& cstpath)
        {
            // First create a new rack
            auto cmd_rack = new Scenario::Command::AddRackToConstraint{Path<ConstraintModel>{cstpath}};
            addCommand(cmd_rack);

            auto createdRackPath = cstpath.extend(RackModel::className, cmd_rack->createdRack());

            // Then create a slot in this rack
            auto cmd_slot = new Scenario::Command::AddSlotToRack{Path<RackModel>{createdRackPath}};
            addCommand(cmd_slot);

            auto createdSlotPath = createdRackPath.extend(SlotModel::className, cmd_slot->createdSlot());
            slotsToUse.push_back({std::move(createdSlotPath), {}});

            // Finally show the rack.
            addCommand(new ShowRackInAllViewModels{Path<ConstraintModel>{cstpath}, cmd_rack->createdRack()});
        }

        // Use this constructor when the constraint already exists
        AddMultipleProcessesToConstraintMacro(const ConstraintModel& constraint)
        {
            Path<ConstraintModel> cstpath{constraint};

            if(constraint.racks.size() == 0)
            {
                auto cmd_rack = new Scenario::Command::AddRackToConstraint{constraint};
                addCommand(cmd_rack);

                auto createdRackPath = cstpath.extend(RackModel::className, cmd_rack->createdRack());

                auto cmd_slot = new Scenario::Command::AddSlotToRack{Path<RackModel>{createdRackPath}};
                addCommand(cmd_slot);

                auto createdSlotPath = createdRackPath.extend(SlotModel::className, cmd_slot->createdSlot());
                slotsToUse.push_back({std::move(createdSlotPath), {}});


                for(const auto& vm : constraint.viewModels())
                {
                    auto cmd_showrack = new Scenario::Command::ShowRackInViewModel{
                                            *vm,
                                            cmd_rack->createdRack()};
                    addCommand(cmd_showrack);
                }
            }
            else
            {
                // For each view model,
                //  if it has a rack
                //  then
                //    if it has a slot
                //    then
                //      show our curves in the upper slot
                //    else
                //      create a slot
                //      show our curves in this slot
                //  else
                //  create a new empty rack
                //  create a new slot
                //  show this rack

                // The rack that we may have to add.
                auto cmd_rack = new Scenario::Command::AddRackToConstraint{constraint};
                bool cmd_rack_used = std::any_of(constraint.viewModels().begin(),
                                                 constraint.viewModels().end(),
                                                 [] (const auto& vm) { return vm->shownRack(); });
                if(cmd_rack_used)
                {
                    addCommand(cmd_rack);

                    auto createdRackPath = cstpath.extend(RackModel::className, cmd_rack->createdRack());

                    auto cmd_slot = new Scenario::Command::AddSlotToRack{Path<RackModel>{createdRackPath}};
                    addCommand(cmd_slot);

                    auto createdSlotPath = createdRackPath.extend(SlotModel::className, cmd_slot->createdSlot());
                    slotsToUse.push_back({std::move(createdSlotPath), {}});
                }

                for(const ConstraintViewModel* vm : constraint.viewModels())
                {
                    if(auto rackId = vm->shownRack())
                    {
                        auto& rack = constraint.racks.at(rackId);
                        if(rack.slotmodels.size() > 0)
                        {
                            // Check if the rack / slot has already been added
                            for(const auto& elt : slotsToUse)
                            {
                                const ObjectIdentifierVector& vec = elt.first.unsafePath().vec();
                                if(vec[vec.size() - 2].id() == rackId.val())
                                    continue;
                            }

                            // If not, we add it to the list
                            slotsToUse.push_back({rack.slotmodels.at(rack.slotsPositions()[0]), {}});
                        }
                        else
                        {
                            Path<RackModel> rackPath{rack};
                            auto cmd_slot = new Scenario::Command::AddSlotToRack{Path<RackModel>{rackPath}};
                            addCommand(cmd_slot);

                            auto createdSlotPath = rackPath.extend(SlotModel::className, cmd_slot->createdSlot());
                            slotsToUse.push_back({std::move(createdSlotPath), {}});
                        }
                    }
                    else
                    {
                        // Show the created rack
                        auto cmd_showrack = new Scenario::Command::ShowRackInViewModel{*vm, cmd_rack->createdRack()};
                        addCommand(cmd_showrack);
                    }
                }

                if(!cmd_rack_used)
                {
                    delete cmd_rack;
                }
            }
        }

        std::vector<std::pair<Path<SlotModel>, std::vector<Id<LayerModel>>>> slotsToUse; // No need to save this, it is useful only for construction.
};


#include <iscore/tools/SettableIdentifierGeneration.hpp>
inline std::tuple<
    AddMultipleProcessesToConstraintMacro*,
    std::vector<std::vector<std::pair<Path<SlotModel>, Id<LayerModel>>>>
>
makeAddProcessMacro(
        const ConstraintModel& constraint,
        int num_processes)
{
    auto macro = new AddMultipleProcessesToConstraintMacro{constraint}; // The constraint already exists

    std::vector<std::pair<Path<SlotModel>, std::vector<Id<LayerModel>>>> slotVec;
    slotVec.reserve(macro->slotsToUse.size());
    // For each slot we have to generate num_processes ids.
    for(const auto& elt : macro->slotsToUse)
    {
        if(auto slot = elt.first.try_find())
        {
            slotVec.push_back({elt.first, getStrongIdRange<LayerModel>(num_processes, slot->layers)});
        }
        else
        {
            slotVec.push_back({elt.first, getStrongIdRange<LayerModel>(num_processes)});
        }
    }

    std::vector<std::vector<std::pair<Path<SlotModel>, Id<LayerModel>>>> bigLayerVec;
    bigLayerVec.resize(num_processes);
    for(int i = 0; i < num_processes; ++i)
    {
        auto& layerVec = bigLayerVec[i];
        layerVec.reserve(macro->slotsToUse.size());
        std::transform(slotVec.begin(), slotVec.end(), std::back_inserter(layerVec),
                       [&] (const auto& slotVecElt) {
           return std::make_pair(slotVecElt.first, slotVecElt.second[i]);
        });
    }

    return std::make_tuple(macro, std::move(bigLayerVec));
}
