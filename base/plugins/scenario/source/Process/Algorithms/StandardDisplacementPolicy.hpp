#pragma once
#include <ProcessInterface/TimeValue.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <Process/ScenarioModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>


void translateNextElements(ScenarioModel& scenario,
                           id_type<TimeNodeModel> firstTimeNodeMovedId,
                           TimeValue deltaTime,
                           QVector<id_type<EventModel>>& movedEvents);


namespace StandardDisplacementPolicy
{
    // pick out each timeNode that need to move when firstTimeNodeMovedId is moving
    void getRelatedElements(ScenarioModel& scenario,
                        id_type<TimeNodeModel> firstTimeNodeMovedId,
                        QVector<id_type<TimeNodeModel> >& translatedTimeNodes);


    template<typename ProcessScaleMethod>
    void updatePositions(ScenarioModel& scenario,
                          QVector<id_type<TimeNodeModel> > translatedTimeNodes,
                          TimeValue deltaTime,
                          ProcessScaleMethod&& scaleMethod)
    {
        for (auto timeNode_id :translatedTimeNodes)
        {
            TimeNodeModel* timeNode = scenario.timeNode(timeNode_id);
            timeNode->setDate(timeNode->date() + deltaTime);
            for (auto event : timeNode->events())
            {
                scenario.event(event)->setDate(timeNode->date());
                emit scenario.eventMoved(event);
            }
        }
        for(ConstraintModel* constraint : scenario.constraints())
        {
            auto startEventDate = scenario.event(constraint->startEvent())->date();
            auto endEventDate = scenario.event(constraint->endEvent())->date();

            TimeValue newDuration = endEventDate - startEventDate;

            if ( !(constraint->startDate() - startEventDate).isZero())
            {
                constraint->setStartDate(startEventDate);
            }
            if(! (constraint->defaultDuration() - newDuration).isZero())
            {
                ConstraintModel::Algorithms::changeAllDurations(*constraint, newDuration);
                for(auto& process : constraint->processes())
                {
                    scaleMethod(process, newDuration);
                }
            }

            emit scenario.constraintMoved(constraint->id());
        }
    }

    // ProcessScaleMethod is a callable object that takes a ProcessModel*
    template<typename ProcessScaleMethod>
    void setEventPosition(ScenarioModel& scenario,
                          id_type<EventModel> eventId,
                          TimeValue absolute_time,
                          double heightPosition,
                          ProcessScaleMethod&& scaleMethod)
    {
        auto ev = scenario.event(eventId);

        if (*ev->timeNode().val() == 0)
            return;

        // don't touch start event
        if(! ev->previousConstraints().isEmpty())
        {
            TimeValue time = absolute_time - ev->date();
            ev->setHeightPercentage(heightPosition);

            // algo
            QVector<id_type<EventModel>> already_moved_events;
            translateNextElements(scenario,
                                  ev->timeNode(),
                                  time,
                                  already_moved_events);

            // update constraints size
            for(ConstraintModel* constraint : scenario.constraints())
            {
                auto startEventDate = scenario.event(constraint->startEvent())->date();
                auto endEventDate = scenario.event(constraint->endEvent())->date();

                TimeValue newDuration = endEventDate - startEventDate;

                if(! (constraint->defaultDuration() - newDuration).isZero())
                {
                    constraint->setStartDate(startEventDate);

                    ConstraintModel::Algorithms::changeAllDurations(*constraint, newDuration);
                    for(auto& process : constraint->processes())
                    {
                        scaleMethod(process, newDuration);
                    }
                    emit scenario.constraintMoved(constraint->id());
                }
            }
        }
    }

    template<typename ProcessScaleMethod>
    void setConstraintPosition(ScenarioModel& scenario,
                               id_type<ConstraintModel> constraintId,
                               TimeValue absolute_time,
                               double heightPosition,
                               ProcessScaleMethod&& scaleMethod)
    {
        auto constraint = scenario.constraint(constraintId);
        constraint->setHeightPercentage(heightPosition);
        emit scenario.constraintMoved(constraintId);

        auto sev = scenario.event(constraint->startEvent());

        if(sev->date() != absolute_time)
        {
            StandardDisplacementPolicy::setEventPosition(scenario,
                                                         sev->id(),
                                                         absolute_time,
                                                         sev->heightPercentage(),
                                                         scaleMethod);
        }
    }
}
