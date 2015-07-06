#include "StandardDisplacementPolicy.hpp"

void StandardDisplacementPolicy::getRelatedTimeNodes(
        ScenarioModel& scenario,
        const id_type<TimeNodeModel>& firstTimeNodeMovedId,
        QVector<id_type<TimeNodeModel> >& translatedTimeNodes)
{
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

        for(const auto& state_id : cur_event.states())
        {
            auto& state = scenario.state(state_id);
            if(state.nextConstraint())
            {
                auto cons = state.nextConstraint();
                auto endStateId = scenario.constraint(cons).endState();
                auto endTnId = scenario.event(scenario.state(endStateId).eventId()).timeNode();
                getRelatedTimeNodes(scenario, endTnId, translatedTimeNodes);
            }
        }
    }
}
