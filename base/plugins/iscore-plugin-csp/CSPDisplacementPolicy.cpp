#include <CSPDisplacementPolicy.hpp>
#include <Model/CSPScenario.hpp>
#include <Model/CSPTimeNode.hpp>
#include <Model/CSPTimeRelation.hpp>

#define STAY_MINMAXFROMDATEONCREATION_STRENGTH kiwi::strength::required
#define STAY_MINMAXPREVTIMERELATION_STRENGTH kiwi::strength::strong
#define STAY_MINMAX_STRENGTH kiwi::strength::required
#define STAY_TNODE_STRENGTH kiwi::strength::medium
#define STAY_DRAGGED_TNODE_STRENGTH kiwi::strength::strong + 1.0 //not so sure that its working


CSPDisplacementPolicy::CSPDisplacementPolicy(ScenarioModel& scenario, const QVector<Id<TimeNodeModel> >& draggedElements)
{
    if(CSPScenario* cspScenario = scenario.findChild<CSPScenario*>("CSPScenario", Qt::FindDirectChildrenOnly))
    {
        // add stays to all elements
        refreshStays(*cspScenario, draggedElements);

    }else
    {
        throw std::runtime_error("No CSP scenario found for this model");
    }
}

void CSPDisplacementPolicy::computeDisplacement(
        ScenarioModel& scenario,
        const QVector<Id<TimeNodeModel>>& draggedElements,
        const TimeValue& deltaTime,
        ElementsProperties& elementsProperties)
{
    // get the csp scenario

    if(CSPScenario* cspScenario = scenario.findChild<CSPScenario*>("CSPScenario", Qt::FindDirectChildrenOnly))
    {
        auto& solver = cspScenario->getSolver();

        // get the corresponding CSP elements and start editing vars
        for(auto curDraggedTimeNodeId : draggedElements)
        {
            auto curDraggedCspTimeNode = cspScenario->m_timeNodes[curDraggedTimeNodeId];

            // get the initial value
            TimeValue initialDate;
            if(elementsProperties.timenodes.contains(curDraggedTimeNodeId))
            {
                initialDate = elementsProperties.timenodes[curDraggedTimeNodeId].oldDate;
            }else
            {
                initialDate = *(curDraggedCspTimeNode->m_iscoreDate);
            }

            //weight
            solver.addEditVariable(curDraggedCspTimeNode->m_date, STAY_DRAGGED_TNODE_STRENGTH);

            // suggest their new values
            auto newDate = initialDate.msec() + deltaTime.msec();
            solver.suggestValue(curDraggedCspTimeNode->getDate(), newDate);
        }

        // solve system
        solver.updateVariables();

        // release edit vars
        for(auto curDraggedTimeNodeId : draggedElements)
        {
            solver.removeEditVariable(cspScenario->m_timeNodes[curDraggedTimeNodeId]->m_date);
        }

        // look for changes // TODO : maybe find a more efficient way of doing that

        // - in timenodes :
        QHashIterator<Id<TimeNodeModel>, CSPTimeNode*> timeNodeIterator(cspScenario->m_timeNodes);
        while (timeNodeIterator.hasNext())
        {
            timeNodeIterator.next();

            auto curTimeNodeId = timeNodeIterator.key();
            auto curCspTimenode = timeNodeIterator.value();

            // if updated TODO : if updated => curCspTimenode->dateChanged()
            if(true)
            {
                // if timenode NOT already in element properties, create new element properties and set the old date
                if(! elementsProperties.timenodes.contains(curTimeNodeId))
                {
                    elementsProperties.timenodes[curTimeNodeId] = TimenodeProperties{};
                    elementsProperties.timenodes[curTimeNodeId].oldDate = *(curCspTimenode->m_iscoreDate);
                }

                // put the new date
                elementsProperties.timenodes[curTimeNodeId].newDate = TimeValue::fromMsecs(curCspTimenode->m_date.value());
            }
        }

        // - in time relations :
        QHashIterator<Id<ConstraintModel>, CSPTimeRelation*> timeRelationIterator(cspScenario->m_timeRelations);
        while(timeRelationIterator.hasNext())
        {
            timeRelationIterator.next();

            const auto& curTimeRelationId = timeRelationIterator.key();
            auto& curCspTimerelation = timeRelationIterator.value();
            const auto& curConstraint = scenario.constraint(curTimeRelationId);

            // if osef TODO : if updated
            if(true)
            {
                // if timenode NOT already in element properties, create new element properties and set the old values
                if(! elementsProperties.constraints.contains(curTimeRelationId))
                {
                    elementsProperties.constraints[curTimeRelationId] = ConstraintProperties{};
                    elementsProperties.constraints[curTimeRelationId].oldMin = *(curCspTimerelation->m_iscoreMin);
                    elementsProperties.constraints[curTimeRelationId].oldMax = *(curCspTimerelation->m_iscoreMax);

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

                    elementsProperties.constraints[curTimeRelationId].savedDisplay = {{iscore::IDocument::path(curConstraint), arr}, map};
                    // Save the constraint display data END ----------------
                }

                // put the new values
                /*
                qDebug() << "----       min : " << curCspTimerelation->m_min.value();
                qDebug() << "    min iscore : " << curCspTimerelation->m_iscoreMin->msec();

                qDebug() << "----       max : " << curCspTimerelation->m_max.value();
                qDebug() << "    max iscore : " << curCspTimerelation->m_iscoreMax->msec();
                */

                elementsProperties.constraints[curTimeRelationId].newMin = TimeValue::fromMsecs(curCspTimerelation->m_min.value());
                elementsProperties.constraints[curTimeRelationId].newMax = TimeValue::fromMsecs(curCspTimerelation->m_max.value());
            }
        }
    }else
    {
        std::runtime_error("No CSP scenario found for this model");
    }
}

void CSPDisplacementPolicy::refreshStays(CSPScenario& cspScenario, const QVector<Id<TimeNodeModel> >& draggedElements)
{
    // time relations stays
    QHashIterator<Id<ConstraintModel>, CSPTimeRelation*> timeRelationIterator(cspScenario.m_timeRelations);
    while(timeRelationIterator.hasNext())
    {
        timeRelationIterator.next();

        auto& curTimeRelationId = timeRelationIterator.key();
        auto& curTimeRelation = timeRelationIterator.value();

        auto initialMin = cspScenario.getScenario()->constraint(curTimeRelationId).duration.minDuration();
        auto initialMax = cspScenario.getScenario()->constraint(curTimeRelationId).duration.maxDuration();

        // - remove old stays
        curTimeRelation->removeStays();

        //ad new stays
        // - if constraint preceed dragged element
        auto endTimeNodeId = cspScenario.getScenario()->constraint(curTimeRelationId).endTimeNode();
        if( draggedElements.contains(endTimeNodeId) )
        {
            auto endTimenode = cspScenario.m_timeNodes[endTimeNodeId];
            auto endDateMsec = endTimenode->m_iscoreDate->msec();
            auto distanceFromMinToDate = endDateMsec - curTimeRelation->m_iscoreMin->msec();
            auto distanceFromMaxToDate = endDateMsec - curTimeRelation->m_iscoreMax->msec();

            // keep min and max around default duration
            curTimeRelation->addStay(new kiwi::Constraint(endTimenode->m_date - curTimeRelation->m_min == distanceFromMinToDate, STAY_MINMAXFROMDATEONCREATION_STRENGTH));
            curTimeRelation->addStay(new kiwi::Constraint(endTimenode->m_date - curTimeRelation->m_max == distanceFromMaxToDate, STAY_MINMAXFROMDATEONCREATION_STRENGTH));

        }else
        {
            curTimeRelation->addStay(new kiwi::Constraint(curTimeRelation->m_min == initialMin.msec(), STAY_MINMAX_STRENGTH));
            curTimeRelation->addStay(new kiwi::Constraint(curTimeRelation->m_max == initialMax.msec(), STAY_MINMAX_STRENGTH));
        }
    }

    //time node stays
    // - in timenodes :
    QHashIterator<Id<TimeNodeModel>, CSPTimeNode*> timeNodeIterator(cspScenario.m_timeNodes);
    while (timeNodeIterator.hasNext())
    {
        timeNodeIterator.next();

        auto& curTimeNodeId = timeNodeIterator.key();
        auto& curCspTimeNode = timeNodeIterator.value();

        // try to stay on initial value
        auto initialDate = cspScenario.getScenario()->timeNode(curTimeNodeId).date();

        // - remove old stays
        curCspTimeNode->removeStays();

        // - add new stays
        curCspTimeNode->addStay(new kiwi::Constraint(curCspTimeNode->m_date == initialDate.msec(), STAY_TNODE_STRENGTH));
    }
}
