#include "StandardDisplacementPolicy.hpp"


void StandardDisplacementPolicy::getRelatedTimeNodes(
        ScenarioModel& scenario,
        const Id<TimeNodeModel>& firstTimeNodeMovedId,
        QVector<Id<TimeNodeModel> >& translatedTimeNodes)
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

//----------------------------------------------------------------------------------------------

void
GoodOldDisplacementPolicy::computeDisplacement(
        ScenarioModel& scenario,
        const QVector<Id<TimeNodeModel>>& draggedElements,
        const TimeValue& deltaTime,
        ElementsProperties& elementsProperties)
{
    // this old behavior supports only the move of one timenode
    if(draggedElements.length() != 1)
    {
        //TODO: log something?
        // move nothing, nothing to undo or redo
        return;
    }else
    {
        const Id<TimeNodeModel>& firstTimeNodeMovedId = draggedElements.at(0);
        QVector<Id<TimeNodeModel> > timeNodesToTranslate;

        StandardDisplacementPolicy::getRelatedTimeNodes(scenario, firstTimeNodeMovedId, timeNodesToTranslate);

        // put each concerned timenode in modified elements
        for(auto& curTimeNodeId : timeNodesToTranslate)
        {
            auto& curTimeNode = scenario.timeNode(curTimeNodeId);

            // if timenode NOT already in element properties, create new element properties and set the old date
            if(! elementsProperties.timenodes.contains(curTimeNodeId))
            {
                elementsProperties.timenodes[curTimeNodeId] = TimenodeProperties{};
                elementsProperties.timenodes[curTimeNodeId].oldDate = curTimeNode.date();
            }

            // put the new date
            elementsProperties.timenodes[curTimeNodeId].newDate = curTimeNode.date() + deltaTime;

            // Make a list of the constraints that need to be resized
            for(const auto& ev_id : curTimeNode.events())
            {
                const auto& ev = scenario.event(ev_id);
                for(const auto& st_id : ev.states())
                {
                    const auto& st = scenario.state(st_id);
                    if(auto& curConstraintId = st.previousConstraint())
                    {
                        auto& curConstraint = scenario.constraint(curConstraintId);

                        // if timenode NOT already in element properties, create new element properties and set old values
                        if(! elementsProperties.constraints.contains(curConstraintId))
                        {
                            elementsProperties.constraints[curConstraintId] = ConstraintProperties{};
                            elementsProperties.constraints[curConstraintId].oldMin = curConstraint.duration.minDuration();
                            elementsProperties.constraints[curConstraintId].oldMax = curConstraint.duration.maxDuration();

                            // Save the constraint display data START ----------------
                            QByteArray arr;
                            Visitor<Reader<DataStream>> jr{&arr};
                            jr.readFrom(curConstraint);

                            // Save for each view model of this constraint
                            // the identifier of the rack that was displayed
                            QMap<Id<ConstraintViewModel>, Id<RackModel>> map;
                            for(const ConstraintViewModel* vm : curConstraint.viewModels())
                            {
                                map[vm->id()] = vm->shownRack();
                            }

                            elementsProperties.constraints[curConstraintId].savedDisplay = {{iscore::IDocument::path(curConstraint), arr}, map};
                            // Save the constraint display data END ----------------
                        }

                        elementsProperties.constraints[curConstraintId].newMin = curConstraint.duration.minDuration() + deltaTime;
                        elementsProperties.constraints[curConstraintId].newMax = curConstraint.duration.maxDuration() + deltaTime;

                        // nothing to do for now
                    }
                }
            }
        }
    }
}
