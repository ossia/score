#pragma once
#include <ProcessInterface/TimeValue.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <Process/ScenarioModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>

namespace StandardDisplacementPolicy
{
    // pick out each timeNode that need to move when firstTimeNodeMovedId is moving
    void getRelatedTimeNodes(
            ScenarioModel& scenario,
            const id_type<TimeNodeModel>& firstTimeNodeMovedId,
            QVector<id_type<TimeNodeModel> >& translatedTimeNodes);


    template<typename ProcessScaleMethod>
    void updatePositions(
            ScenarioModel& scenario,
            const QVector<id_type<TimeNodeModel> >& translatedTimeNodes,
            const TimeValue& deltaTime,
            ProcessScaleMethod&& scaleMethod)
    {
        for (const auto& timeNode_id : translatedTimeNodes)
        {
            auto& timeNode = scenario.timeNode(timeNode_id);
            timeNode.setDate(timeNode.date() + deltaTime);
            for (const auto& event : timeNode.events())
            {
                scenario.event(event).setDate(timeNode.date());
            }
        }

        for(auto& constraint : scenario.constraints())
        {
            const auto& startDate = scenario.event(scenario.state(constraint.startState()).eventId()).date();
            const auto& endDate = scenario.event(scenario.state(constraint.endState()).eventId()).date();

            TimeValue newDuration = endDate - startDate;

            if (!(constraint.startDate() - startDate).isZero())
            {
                constraint.setStartDate(startDate);
            }

            if(!(constraint.duration.defaultDuration() - newDuration).isZero())
            {
                ConstraintDurations::Algorithms::setDurationInBounds(constraint, newDuration);
                for(auto& process : constraint.processes())
                {
                    scaleMethod(process, newDuration);
                }
            }

            emit scenario.constraintMoved(constraint);
        }
    }
}
