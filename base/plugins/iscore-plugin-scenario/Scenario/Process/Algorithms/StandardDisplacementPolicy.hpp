#pragma once
#include <Process/TimeValue.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <Scenario/Commands/Scenario/Deletions/ClearConstraint.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>

#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Process/LayerModel.hpp>

#include <Scenario/Tools/dataStructures.hpp>

using namespace Scenario::Command;

/**
 * @brief The displacementPolicy class
 * This class allows to implement multiple displacement behaviors.
 */
class CommonDisplacementPolicy
{
    public:

        template<typename ProcessScaleMethod>
        static
        void
        updatePositions(
                Scenario::ScenarioModel& scenario,
                ProcessScaleMethod&& scaleMethod,
                const ElementsProperties& elementsPropertiesToUpdate)
        {
            // update each affected timenodes
            for(auto& curTimenodePropertiesToUpdate_id : elementsPropertiesToUpdate.timenodes.keys())
            {
                auto& curTimenodeToUpdate = scenario.timeNode(curTimenodePropertiesToUpdate_id);
                auto& curTimenodePropertiesToUpdate = elementsPropertiesToUpdate.timenodes[curTimenodePropertiesToUpdate_id];


                curTimenodeToUpdate.setDate(curTimenodePropertiesToUpdate.newDate);


                // update related events
                for (const auto& event : curTimenodeToUpdate.events())
                {
                    scenario.events.at(event).setDate(curTimenodeToUpdate.date());
                }
            }

            // update affected constraints
            QMapIterator<Id<ConstraintModel>, ConstraintProperties> constraintPropertiesIterator(elementsPropertiesToUpdate.constraints);
            while (constraintPropertiesIterator.hasNext())
            {
                constraintPropertiesIterator.next();

                auto curConstraintPropertiesToUpdate_id = constraintPropertiesIterator.key();

                auto& curConstraintToUpdate = scenario.constraints.at(curConstraintPropertiesToUpdate_id);
                auto& curConstraintPropertiesToUpdate = constraintPropertiesIterator.value();

                // compute default duration here
                const auto& startDate = scenario.event(scenario.state(curConstraintToUpdate.startState()).eventId()).date();
                const auto& endDate = scenario.event(scenario.state(curConstraintToUpdate.endState()).eventId()).date();

                TimeValue defaultDuration = endDate - startDate;

                // set start date and default duration
                if (!(curConstraintToUpdate.startDate() - startDate).isZero())
                {
                    curConstraintToUpdate.setStartDate(startDate);
                }
                curConstraintToUpdate.duration.setDefaultDuration(defaultDuration);


                curConstraintToUpdate.duration.setMinDuration(curConstraintPropertiesToUpdate.newMin);
                curConstraintToUpdate.duration.setMaxDuration(curConstraintPropertiesToUpdate.newMax);

                for(auto& process : curConstraintToUpdate.processes)
                {
                    scaleMethod(process, defaultDuration);
                }



                emit scenario.constraintMoved(curConstraintToUpdate);
            }
        }

        template<typename ProcessScaleMethod>
        static
        void
        revertPositions(
                Scenario::ScenarioModel& scenario,
                ProcessScaleMethod&& scaleMethod,
                const ElementsProperties& elementsPropertiesToUpdate)
        {
            // update each affected timenodes with old values
            for(auto& curTimenodePropertiesToUpdate_id : elementsPropertiesToUpdate.timenodes.keys())
            {
                auto& curTimenodeToUpdate = scenario.timeNode(curTimenodePropertiesToUpdate_id);
                auto& curTimenodePropertiesToUpdate = elementsPropertiesToUpdate.timenodes[curTimenodePropertiesToUpdate_id];


                curTimenodeToUpdate.setDate(curTimenodePropertiesToUpdate.oldDate);

                // update related events to mach the date
                for (const auto& event : curTimenodeToUpdate.events())
                {
                    scenario.events.at(event).setDate(curTimenodeToUpdate.date());
                }
            }

            // update affected constraints with old values and restor processes
            QMapIterator<Id<ConstraintModel>, ConstraintProperties> constraintPropertiesIterator(elementsPropertiesToUpdate.constraints);
            while (constraintPropertiesIterator.hasNext())
            {
                constraintPropertiesIterator.next();

                auto curConstraintPropertiesToUpdate_id = constraintPropertiesIterator.key();

                auto& curConstraintToUpdate = scenario.constraints.at(curConstraintPropertiesToUpdate_id);
                auto& curConstraintPropertiesToUpdate = constraintPropertiesIterator.value();

                // compute default duration here
                const auto& startDate = scenario.event(scenario.state(curConstraintToUpdate.startState()).eventId()).date();
                const auto& endDate = scenario.event(scenario.state(curConstraintToUpdate.endState()).eventId()).date();

                TimeValue defaultDuration = endDate - startDate;

                // set start date and default duration
                if (!(curConstraintToUpdate.startDate() - startDate).isZero())
                {
                    curConstraintToUpdate.setStartDate(startDate);
                }
                curConstraintToUpdate.duration.setDefaultDuration(defaultDuration);

                // set durations
                curConstraintToUpdate.duration.setMinDuration(curConstraintPropertiesToUpdate.oldMin);
                curConstraintToUpdate.duration.setMaxDuration(curConstraintPropertiesToUpdate.oldMax);

                // Now we have to restore the state of each constraint that might have been modified
                // during this command.
                auto& savedConstraintData = elementsPropertiesToUpdate.constraints[curConstraintPropertiesToUpdate_id].savedDisplay;

                // 1. Clear the constraint
                // TODO Don't use a command since it serializes a ton of unused stuff.
                ClearConstraint clear_cmd{Path<ConstraintModel>{savedConstraintData.first.first}};
                clear_cmd.redo();

                auto& constraint = savedConstraintData.first.first.find();
                // 2. Restore the rackes & processes.

                // TODO if possible refactor this with ReplaceConstraintContent and ConstraintModel::clone
                // Be careful however, the code differs in subtle ways
                {
                    ConstraintModel src_constraint{
                        Deserializer<DataStream>{savedConstraintData.first.second},
                        &constraint}; // Temporary parent

                    std::map<const Process::ProcessModel*, Process::ProcessModel*> processPairs;

                    // Clone the processes
                    for(const auto& sourceproc : src_constraint.processes)
                    {
                        auto newproc = sourceproc.clone(sourceproc.id(), &constraint);

                        processPairs.insert(std::make_pair(&sourceproc, newproc));
                        AddProcess(constraint, newproc);
                    }

                    // Clone the rackes
                    for(const auto& sourcerack : src_constraint.racks)
                    {
                        // A note about what happens here :
                        // Since we want to duplicate our process view models using
                        // the target constraint's cloned shared processes (they might setup some specific data),
                        // we maintain a pair mapping each original process to their cloned counterpart.
                        // We can then use the correct cloned process to clone the process view model.
                        auto newrack = new RackModel{
                                       sourcerack,
                                       sourcerack.id(),
                                       [&] (const SlotModel& source, SlotModel& target)
                        {
                            for(const auto& lm : source.layers)
                            {
                                // We can safely reuse the same id since it's in a different slot.
                                Process::ProcessModel* proc = processPairs[&lm.processModel()];
                                // TODO harmonize the order of parameters (source first, then new id)
                                target.layers.add(proc->cloneLayer(lm.id(), lm, &target));
                            }
                        },
                        &constraint};
                        constraint.racks.add(newrack);
                    }
                }

                // 3. Restore the correct rackes in the constraint view models
                if(constraint.processes.size() != 0)
                {
                    for(auto& viewmodel : constraint.viewModels())
                    {
                        viewmodel->showRack(curConstraintPropertiesToUpdate.savedDisplay.second[viewmodel->id()]);
                    }
                }


                emit scenario.constraintMoved(curConstraintToUpdate);
            }
        }
};
