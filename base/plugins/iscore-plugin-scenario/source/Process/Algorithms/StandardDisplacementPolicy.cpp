#include "StandardDisplacementPolicy.hpp"


void translateNextElements(
        ScenarioModel& scenario,
        const id_type<TimeNodeModel>& firstTimeNodeMovedId,
        const TimeValue& deltaTime,
        QVector<id_type<EventModel>>& movedEvents)
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    /*
    if (*firstTimeNodeMovedId.val() == 0 || *firstTimeNodeMovedId.val() == 1 )
        return;
    auto& cur_timeNode = scenario.timeNode(firstTimeNodeMovedId);

    for(const auto& cur_eventId : cur_timeNode.events())
    {
        auto& cur_event = scenario.event(cur_eventId);

        if(movedEvents.indexOf(cur_eventId) == -1)
        {
            cur_event.translate(deltaTime);
            movedEvents.push_back(cur_eventId);
            cur_timeNode.setDate(cur_event.date());
            emit scenario.eventMoved(cur_eventId);
        }

        // if current event is'nt the StartEvent
        for(const auto& cons : cur_event.nextConstraints())
        {
            const auto& evId = scenario.constraint(cons).endEvent();

            // if event has not already moved
            if(movedEvents.indexOf(evId) == -1
                    && scenario.event(evId).timeNode() != scenario.startEvent().timeNode())
            {
                scenario.event(evId).translate(deltaTime);
                movedEvents.push_back(evId);
                scenario.constraint(cons).translate(deltaTime);

                // move timeNode
                auto& tn = scenario.timeNode(scenario.event(evId).timeNode());
                tn.setDate(scenario.event(evId).date());

                emit scenario.eventMoved(evId);
                emit scenario.constraintMoved(cons);

                translateNextElements(scenario, tn.id(), deltaTime, movedEvents);
            }
        }
    }
    */
}

void StandardDisplacementPolicy::getRelatedElements(
        ScenarioModel& scenario,
        const id_type<TimeNodeModel>& firstTimeNodeMovedId,
        QVector<id_type<TimeNodeModel> >& translatedTimeNodes)
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    /*
    if (*firstTimeNodeMovedId.val() == 0 || *firstTimeNodeMovedId.val() == 1 )
        return;

    if(translatedTimeNodes.indexOf(firstTimeNodeMovedId) == -1)
    {
        translatedTimeNodes.push_back(firstTimeNodeMovedId);
    }
    else // timeNode already moved
    {
        return;
    }

    const auto& cur_timeNode = scenario.timeNode(firstTimeNodeMovedId);
    for(const auto& cur_eventId : cur_timeNode.events())
    {
        const auto& cur_event = scenario.event(cur_eventId);

        for(const auto& cons : cur_event.nextConstraints())
        {
            auto endEvId = scenario.constraint(cons).endEvent();
            auto endTnId = scenario.event(endEvId).timeNode();
            getRelatedElements(scenario, endTnId, translatedTimeNodes);
        }
    }
    */
}
