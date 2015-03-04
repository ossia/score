#include "StandardDisplacementPolicy.hpp"


void translateNextElements(ScenarioModel& scenario,
                           id_type<TimeNodeModel> firstTimeNodeMovedId,
                           TimeValue deltaTime,
                           QVector<id_type<EventModel>>& movedEvents)
{
    auto cur_timeNode = scenario.timeNode(firstTimeNodeMovedId);

    for(id_type<EventModel> cur_eventId : cur_timeNode->events())
    {
        EventModel* cur_event = scenario.event(cur_eventId);

        if(movedEvents.indexOf(cur_eventId) == -1)
        {
            cur_event->translate(deltaTime);
            movedEvents.push_back(cur_eventId);
            cur_timeNode->setDate(cur_event->date());
            emit scenario.eventMoved(cur_eventId);
        }

        // if current event is'nt the StartEvent
        if(! cur_event->previousConstraints().isEmpty())
        {
            for(id_type<ConstraintModel> cons : cur_event->nextConstraints())
            {
                auto evId = scenario.constraint(cons)->endEvent();

                // if event has not already moved
                if(movedEvents.indexOf(evId) == -1)
                {
                    scenario.event(evId)->translate(deltaTime);
                    movedEvents.push_back(evId);
                    scenario.constraint(cons)->translate(deltaTime);

                    // move timeNode
                    auto tn = scenario.timeNode(scenario.event(evId)->timeNode());
                    tn->setDate(scenario.event(evId)->date());

                    emit scenario.eventMoved(evId);
                    emit scenario.constraintMoved(cons);

                    translateNextElements(scenario, tn->id(), deltaTime, movedEvents);
                }
            }
        }
    }
}
