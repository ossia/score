#include "GoodOldDisplacementPolicy.hpp"

#include <ProcessInterface/TimeValue.hpp>

#include <Process/ScenarioModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>

#include "Tools/dataStructures.hpp"



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
        QVector<Id<TimeNodeModel> > timeNodesToTranslate;

        StandardDisplacementPolicy::getRelatedTimeNodes(scenario, firstTimeNodeMovedId, timeNodesToTranslate);

        // put each concerned timenode in modified elements and compute new values
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
        }

        // Make a list of the constraints that need to be resized
        for(auto& curTimeNodeId : timeNodesToTranslate)
        {
            auto& curTimeNode = scenario.timeNode(curTimeNodeId);

            // each previous constraint
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
