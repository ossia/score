#include "GoodOldDisplacementPolicy.hpp"

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
        qDebug("WARNING : computeDisplacement called with empty element list !");
        // move nothing, nothing to undo or redo
        return;
    }else
    {
        const Id<TimeNodeModel>& firstTimeNodeMovedId = draggedElements.at(0);
        std::vector<Id<TimeNodeModel>> timeNodesToTranslate;

        GoodOldDisplacementPolicy::getRelatedTimeNodes(scenario, firstTimeNodeMovedId, timeNodesToTranslate);

        // put each concerned timenode in modified elements and compute new values
        for(const auto& curTimeNodeId : timeNodesToTranslate)
        {
            auto& curTimeNode = scenario.timeNodes.at(curTimeNodeId);

            // if timenode NOT already in element properties, create new element properties and set the old date
            if(! elementsProperties.timenodes.contains(curTimeNodeId))
            {
                elementsProperties.timenodes[curTimeNodeId] = TimenodeProperties{};
                elementsProperties.timenodes[curTimeNodeId].oldDate = curTimeNode.date();
            }

            // put the new date
            elementsProperties.timenodes[curTimeNodeId].newDate = elementsProperties.timenodes[curTimeNodeId].oldDate + deltaTime;
        }

        // Make a list of the constraints that need to be resized
        for(const auto& curTimeNodeId : timeNodesToTranslate)
        {
            auto& curTimeNode = scenario.timeNode(curTimeNodeId);

            // each previous constraint
            for(const auto& ev_id : curTimeNode.events())
            {
                const auto& ev = scenario.event(ev_id);
                for(const auto& st_id : ev.states())
                {
                    const auto& st = scenario.states.at(st_id);
                    if(const auto& curConstraintId = st.previousConstraint())
                    {
                        auto& curConstraint = scenario.constraints.at(curConstraintId);

                        // if timenode NOT already in element properties, create new element properties and set old values
                        if(! elementsProperties.constraints.contains(curConstraintId))
                        {
                            elementsProperties.constraints[curConstraintId] = ConstraintProperties{};
                            elementsProperties.constraints[curConstraintId].oldMin = curConstraint.duration.minDuration();
                            elementsProperties.constraints[curConstraintId].oldMax = curConstraint.duration.maxDuration();

                            // Save the constraint display data START ----------------
                            QByteArray arr;
                            Visitor<Reader<DataStream> > jr{&arr};
                            jr.readFrom(curConstraint);

                            // Save for each view model of this constraint
                            // the identifier of the rack that was displayed
                            QMap<Id<ConstraintViewModel>, Id<RackModel> > map;
                            for(const ConstraintViewModel* vm : curConstraint.viewModels())
                            {
                                map[vm->id()] = vm->shownRack();
                            }

                            elementsProperties.constraints[curConstraintId].savedDisplay = {{curConstraint, arr}, map};
                            // Save the constraint display data END ----------------

                        }

                        auto& startTnodeId = scenario.event(scenario.state(curConstraint.startState()).eventId()).timeNode();

                        // compute default duration
                        TimeValue startDate;

                        // if prev tnode has moved take updated value else take existing
                        if( elementsProperties.timenodes.contains(startTnodeId) )
                        {
                            startDate = elementsProperties.timenodes[startTnodeId].newDate;
                        }else
                        {
                            startDate = scenario.event(scenario.state(curConstraint.startState()).eventId()).date();
                        }

                        const auto& endDate = elementsProperties.timenodes[curTimeNodeId].newDate;

                        TimeValue newDefaultDuration = endDate - startDate;
                        TimeValue deltaBounds = newDefaultDuration - curConstraint.duration.defaultDuration();

                        elementsProperties.constraints[curConstraintId].newMin = curConstraint.duration.minDuration() + deltaBounds;
                        elementsProperties.constraints[curConstraintId].newMax = curConstraint.duration.maxDuration() + deltaBounds;

                        // nothing to do for now
                    }
                }
            }
        }
    }
}

void GoodOldDisplacementPolicy::getRelatedTimeNodes(
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
