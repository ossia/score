#include "StandardDisplacementPolicy.hpp"


void translateNextElements(ScenarioModel& scenario,
                           id_type<TimeNodeModel> firstTimeNodeMovedId,
                           TimeValue deltaTime,
                           QVector<id_type<EventModel>>& movedEvents)
{
    if (*firstTimeNodeMovedId.val() == 0 || *firstTimeNodeMovedId.val() == 1 )
        return;
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
        for(id_type<ConstraintModel> cons : cur_event->nextConstraints())
        {
            auto evId = scenario.constraint(cons)->endEvent();

            // if event has not already moved
            if(movedEvents.indexOf(evId) == -1 && scenario.event(evId)->timeNode() != 0)
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

void StandardDisplacementPolicy::getRelatedElements(ScenarioModel& scenario,
                        id_type<TimeNodeModel> firstTimeNodeMovedId,
                        QVector<id_type<TimeNodeModel> >& translatedTimeNodes)
{
    if (*firstTimeNodeMovedId.val() == 0 || *firstTimeNodeMovedId.val() == 1 )
        return;
    auto cur_timeNode = scenario.timeNode(firstTimeNodeMovedId);

    if(translatedTimeNodes.indexOf(firstTimeNodeMovedId) == -1)
    {
        translatedTimeNodes.push_back(firstTimeNodeMovedId);
    }
    else // timeNode already moved
    {
        return;
    }

    for(id_type<EventModel> cur_eventId : cur_timeNode->events())
    {
        EventModel* cur_event = scenario.event(cur_eventId);

        for(id_type<ConstraintModel> cons : cur_event->nextConstraints())
        {
            auto endEvId = scenario.constraint(cons)->endEvent();
            auto endTnId = scenario.event(endEvId)->timeNode();
            getRelatedElements(scenario, endTnId, translatedTimeNodes);
        }
    }
}
