#pragma once
#include <iscore/command/AggregateCommand.hpp>
#include <Commands/ScenarioCommandFactory.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Constraint/ViewModels/ConstraintViewModel.hpp>

#include <Commands/Scenario/ShowRackInViewModel.hpp>
#include <Commands/Scenario/ShowRackInAllViewModels.hpp>
#include <Commands/Constraint/AddRackToConstraint.hpp>
#include <Commands/Constraint/Rack/AddSlotToRack.hpp>

// One InterpolateMacro per constraint
class GenericInterpolateMacro : public iscore::AggregateCommand
{
        ISCORE_AGGREGATE_COMMAND_DECL(ScenarioCommandFactoryName(),
                                      GenericInterpolateMacro,
                                      "GenericInterpolateMacro")
};

class InterpolateMacro : public iscore::AggregateCommand
{
        ISCORE_AGGREGATE_COMMAND_DECL(ScenarioCommandFactoryName(),
                                      InterpolateMacro,
                                      "InterpolateMacro")

        public:
            InterpolateMacro& operator=(const InterpolateMacro& other) = default;

        // Use this constructor when the constraint does not exist yet.
        InterpolateMacro(const Path<ConstraintModel>& cstpath):
            iscore::AggregateCommand{factoryName(), commandName(), description()}
        {
            auto cmd_rack = new Scenario::Command::AddRackToConstraint{Path<ConstraintModel>{cstpath}};
            addCommand(cmd_rack);

            auto unsaferackPath = cstpath.unsafePath().vec();
            unsaferackPath.push_back({RackModel::className, cmd_rack->createdRack()});

            Path<RackModel> createdRackPath{
                                   ObjectPath{std::move(unsaferackPath)},
                                   Path<RackModel>::UnsafeDynamicCreation{}};

            auto cmd_slot = new Scenario::Command::AddSlotToRack{Path<RackModel>{createdRackPath}};
            addCommand(cmd_slot);

            auto unsafeSlotPath = createdRackPath.unsafePath().vec();
            unsafeSlotPath.push_back({SlotModel::className, cmd_slot->createdSlot()});

            slotsToUse.push_back({Path<SlotModel>{
                            ObjectPath{std::move(unsafeSlotPath)},
                                      Path<SlotModel>::UnsafeDynamicCreation{}}, {}});

            addCommand(new ShowRackInAllViewModels{Path<ConstraintModel>{cstpath}, cmd_rack->createdRack()});
        }

            // Use this constructor when the constraint already exists
        InterpolateMacro(const ConstraintModel& constraint):
          iscore::AggregateCommand{factoryName(), commandName(), description()}
        {
            auto cstpath = iscore::IDocument::path(constraint);

            if(constraint.racks.size() == 0)
            {
                auto cmd_rack = new Scenario::Command::AddRackToConstraint{constraint};
                addCommand(cmd_rack);

                auto unsaferackPath = cstpath.unsafePath().vec();
                // TODO when refactoring this, if we pass the id we can reuse the type to keep something strong

                unsaferackPath.push_back({RackModel::className, cmd_rack->createdRack()});

                Path<RackModel> createdRackPath{
                                       ObjectPath{std::move(unsaferackPath)},
                                       Path<RackModel>::UnsafeDynamicCreation{}};

                auto cmd_slot = new Scenario::Command::AddSlotToRack{Path<RackModel>{createdRackPath}};
                addCommand(cmd_slot);

                auto unsafeSlotPath = createdRackPath.unsafePath().vec();
                unsafeSlotPath.push_back({SlotModel::className, cmd_slot->createdSlot()});

                slotsToUse.push_back({Path<SlotModel>{
                                ObjectPath{std::move(unsafeSlotPath)},
                                          Path<SlotModel>::UnsafeDynamicCreation{}}, {}});

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

                    auto unsaferackPath = cstpath.unsafePath().vec();
                    unsaferackPath.push_back({RackModel::className, cmd_rack->createdRack()});

                    auto createdRackPath = Path<RackModel>{
                                    ObjectPath{std::move(unsaferackPath)},
                                    Path<RackModel>::UnsafeDynamicCreation{}};

                    auto cmd_slot = new Scenario::Command::AddSlotToRack{Path<RackModel>{createdRackPath}};
                    addCommand(cmd_slot);

                    auto unsafeSlotPath = createdRackPath.unsafePath().vec();
                    unsafeSlotPath.push_back({SlotModel::className, cmd_slot->createdSlot()});

                    slotsToUse.push_back({Path<SlotModel>{
                                    ObjectPath{std::move(unsafeSlotPath)},
                                    Path<SlotModel>::UnsafeDynamicCreation{}}, {}});
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

                            auto unsafeSlotPath = rackPath.unsafePath().vec();
                            unsafeSlotPath.push_back({SlotModel::className, cmd_slot->createdSlot()});

                            slotsToUse.push_back({Path<SlotModel>{
                                            ObjectPath{std::move(unsafeSlotPath)},
                                                      Path<SlotModel>::UnsafeDynamicCreation{}}, {}});
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
