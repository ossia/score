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
#include <Process/ProcessList.hpp>

#include <Scenario/Tools/dataStructures.hpp>

namespace Scenario
{
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
                Scenario::ProcessModel& scenario,
                ProcessScaleMethod&& scaleMethod,
                const ElementsProperties& propsToUpdate)
        {
            // update each affected timenodes
            for(auto it = propsToUpdate.timenodes.cbegin(); it != propsToUpdate.timenodes.cend(); ++it)
            {
                auto& curTimenodeToUpdate = scenario.timeNode(it.key());
                auto& curTimenodePropertiesToUpdate = it.value();

                curTimenodeToUpdate.setDate(curTimenodePropertiesToUpdate.newDate);

                // update related events
                for (const auto& event : curTimenodeToUpdate.events())
                {
                    scenario.events.at(event).setDate(curTimenodeToUpdate.date());
                }
            }

            // update affected constraints
            QMapIterator<Id<ConstraintModel>, ConstraintProperties> constraintPropertiesIterator(propsToUpdate.constraints);
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
                Scenario::ProcessModel& scenario,
                ProcessScaleMethod&& scaleMethod,
                const ElementsProperties& propsToUpdate)
        {
            // update each affected timenodes with old values
            for(auto it = propsToUpdate.timenodes.cbegin(); it != propsToUpdate.timenodes.cend(); ++it)
            {
                auto& curTimenodeToUpdate = scenario.timeNode(it.key());
                auto& curTimenodePropertiesToUpdate = it.value();

                curTimenodeToUpdate.setDate(curTimenodePropertiesToUpdate.oldDate);

                // update related events to mach the date
                for (const auto& event : curTimenodeToUpdate.events())
                {
                    scenario.events.at(event).setDate(curTimenodeToUpdate.date());
                }
            }

            // update affected constraints with old values and restor processes
            QMapIterator<Id<ConstraintModel>, ConstraintProperties> constraintPropertiesIterator(propsToUpdate.constraints);
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

                // 1. Clear the constraint
                // TODO Don't use a command since it serializes a ton of unused stuff.
                Command::ClearConstraint clear_cmd{curConstraintToUpdate};
                clear_cmd.redo();

                // 2. Restore the rackes & processes.
                // Restore the constraint. The saving is done in GenericDisplacementPolicy.
                curConstraintPropertiesToUpdate.reload(curConstraintToUpdate);

                emit scenario.constraintMoved(curConstraintToUpdate);
            }
        }
};

}
