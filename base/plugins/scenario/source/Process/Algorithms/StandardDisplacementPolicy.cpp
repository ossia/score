#include "StandardDisplacementPolicy.hpp"
#include <Process/ScenarioModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>



void translateNextElements (ScenarioModel& scenario,
                            id_type<TimeNodeModel> firstTimeNodeMovedId,
                            TimeValue deltaTime,
                            QVector<id_type<EventModel>>& movedEvents);

void StandardDisplacementPolicy::setEventPosition (ScenarioModel& scenario,
        id_type<EventModel> eventId,
        TimeValue absolute_time,
        double heightPosition)
{
    auto ev = scenario.event (eventId);

    // resize previous constraint
    if (! ev->previousConstraints().isEmpty() )
    {
        TimeValue time = absolute_time - ev->date();
        ev->setHeightPercentage (heightPosition);

        for (auto& prevConstraintId : ev->previousConstraints() )
        {
            auto prevConstraint = scenario.constraint (prevConstraintId);
            prevConstraint->setDefaultDuration (prevConstraint->defaultDuration() + time);
            emit scenario.constraintMoved (prevConstraintId);
        }

        QVector<id_type<EventModel>> already_moved_events;
        translateNextElements (scenario,
                               ev->timeNode(),
                               time,
                               already_moved_events);

        // update constraints size

        for (ConstraintModel* constraint : scenario.constraints() )
        {
            TimeValue startEventDate = scenario.event (constraint->startEvent() )->date();

            if (constraint->startDate() != startEventDate)
            {
                constraint->setStartDate (startEventDate);
            }

            TimeValue newDuration = scenario.event (constraint->endEvent() )->date() - scenario.event (constraint->startEvent() )->date();

            TimeValue delta = constraint->defaultDuration() - newDuration;

            // if ( delta > std::chrono::milliseconds(1) ||  delta < std::chrono::milliseconds(-1) )
            if (! delta.isZero() )
            {
                constraint->setDefaultDuration (newDuration);
                emit scenario.constraintMoved (constraint->id() );
            }
        }

    }
}

void StandardDisplacementPolicy::setConstraintPosition (ScenarioModel& scenario,
        id_type<ConstraintModel> constraintId,
        TimeValue absolute_time,
        double heightPosition)
{
    auto constraint = scenario.constraint (constraintId);
    constraint->setHeightPercentage (heightPosition);
    emit scenario.constraintMoved (constraintId);

    auto sev = scenario.event (constraint->startEvent() );

    if (sev->date() != absolute_time)
    {
        StandardDisplacementPolicy::setEventPosition (scenario,
                sev->id(),
                absolute_time,
                sev->heightPercentage() );
    }
}

void translateNextElements (ScenarioModel& scenario,
                            id_type<TimeNodeModel> firstTimeNodeMovedId,
                            TimeValue deltaTime,
                            QVector<id_type<EventModel>>& movedEvents)
{
    auto cur_timeNode = scenario.timeNode (firstTimeNodeMovedId);

    for (id_type<EventModel> cur_eventId : cur_timeNode->events() )
    {
        EventModel* cur_event = scenario.event (cur_eventId);

        if (movedEvents.indexOf (cur_eventId) == -1)
        {
            cur_event->translate (deltaTime);
            movedEvents.push_back (cur_eventId);
            cur_timeNode->setDate (cur_event->date() );
            emit scenario.eventMoved (cur_eventId);
        }

        // if current event is'nt the StartEvent
        if (! cur_event->previousConstraints().isEmpty() )
        {
            for (id_type<ConstraintModel> cons : cur_event->nextConstraints() )
            {
                auto evId = scenario.constraint (cons)->endEvent();

                // if event has not already moved
                if (movedEvents.indexOf (evId) == -1)
                {
                    scenario.event (evId)->translate (deltaTime);
                    movedEvents.push_back (evId);
                    scenario.constraint (cons)->translate (deltaTime);

                    // move timeNode
                    auto tn = scenario.timeNode (scenario.event (evId)->timeNode() );
                    tn->setDate (scenario.event (evId)->date() );

                    emit scenario.eventMoved (evId);
                    emit scenario.constraintMoved (cons);

                    translateNextElements (scenario, tn->id(), deltaTime, movedEvents);
                }
            }
        }
    }
}
