#include <CSPDisplacementPolicy.hpp>
#include <Model/CSPScenario.hpp>
#include <Model/CSPTimeNode.hpp>
#include <Model/CSPTimeRelation.hpp>

void
CSPDisplacementPolicy::computeDisplacement(
        ScenarioModel& scenario,
        const QVector<Id<TimeNodeModel>>& draggedElements,
        const TimeValue& deltaTime,
        ElementsProperties& elementsProperties)
{
    // get the csp scenario
    CSPScenario* cspScenario = scenario.findChild<CSPScenario*>("CSPScenario", Qt::FindDirectChildrenOnly);

    auto& solver = cspScenario->getSolver();

    // get the corresponding CSP elements
    for(auto curDraggedTimeNodeId : draggedElements)
    {
        auto curDraggedCspTimeNode = cspScenario->timenodes[curDraggedTimeNodeId];

        // suggest their new values
        auto newDate = curDraggedCspTimeNode->getDate().value() + deltaTime.msec();
        solver.suggestValue(curDraggedCspTimeNode->getDate(), newDate);
    }

    // solve system
    solver.updateVariables();

    // look for changes // TODO find a more efficient way of doing that

    // - in timenodes :
    QHashIterator<Id<TimeNodeModel>, CSPTimeNode*> timeNodeIterator(cspScenario->timenodes);
    while (timeNodeIterator.hasNext())
    {
        auto curTimeNodeId = timeNodeIterator.key();
        auto curCspTimenode = timeNodeIterator.value();

        // if update
        if(curCspTimenode->dateChanged())
        {
            // if timenode NOT already in element properties, create new element properties and set the old date
            if(! elementsProperties.timenodes.contains(curTimeNodeId))
            {
                elementsProperties.timenodes[curTimeNodeId] = TimenodeProperties{};
                elementsProperties.timenodes[curTimeNodeId].oldDate = *(curCspTimenode->m_iscoreDate);
            }

            // put the new date
            elementsProperties.timenodes[curTimeNodeId].newDate = TimeValue::fromMsecs(curCspTimenode->m_date.value()) + deltaTime;
        }
    }

    // - in time relations :
}
