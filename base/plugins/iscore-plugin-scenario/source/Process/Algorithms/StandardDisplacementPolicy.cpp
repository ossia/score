#include "StandardDisplacementPolicy.hpp"


void StandardDisplacementPolicy::getRelatedTimeNodes(
        ScenarioModel& scenario,
        const Id<TimeNodeModel>& firstTimeNodeMovedId,
        std::vector<Id<TimeNodeModel> >& translatedTimeNodes)
{
    if (*firstTimeNodeMovedId.val() == 0 || *firstTimeNodeMovedId.val() == 1 )
        return;

    auto it = std::find(translatedTimeNodes.begin(), translatedTimeNodes.end(), firstTimeNodeMovedId);
    if(it == translatedTimeNodes.end())
    {
        translatedTimeNodes.push_back(firstTimeNodeMovedId);
    }
    else // timeNode already moved
    {
        return;
    }

    const auto& cur_timeNode = scenario.timeNodes.at(firstTimeNodeMovedId);
    for(const auto& cur_eventId : cur_timeNode.events())
    {
        const auto& cur_event = scenario.events.at(cur_eventId);

        for(const auto& state_id : cur_event.states())
        {
            const auto& state = scenario.states.at(state_id);
            if(const auto& cons = state.nextConstraint())
            {
                const auto& endStateId = scenario.constraints.at(cons).endState();
                const auto& endTnId = scenario.events.at(scenario.state(endStateId).eventId()).timeNode();
                getRelatedTimeNodes(scenario, endTnId, translatedTimeNodes);
            }
        }
    }
}

