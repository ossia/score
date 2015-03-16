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

            // resize previous constraint
            for(auto& prevConstraintId : ev->previousConstraints())
            {
                auto constraint = scenario.constraint(prevConstraintId);
                auto newDuration = constraint->defaultDuration() + time;

                constraint->setDefaultDuration(newDuration);
                for(auto& process : constraint->processes())
                {
                    scaleMethod(process, newDuration);
                }

                emit scenario.constraintMoved(prevConstraintId);
            }

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

                    constraint->setDefaultDuration(newDuration);
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
