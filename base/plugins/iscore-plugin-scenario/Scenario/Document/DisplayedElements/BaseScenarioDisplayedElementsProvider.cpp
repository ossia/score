#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <QObject>

#include "BaseScenarioDisplayedElementsProvider.hpp"

bool BaseScenarioDisplayedElementsProvider::matches(
        const ConstraintModel& cst) const
{
    return dynamic_cast<BaseScenario*>(cst.parent());
}

DisplayedElementsContainer BaseScenarioDisplayedElementsProvider::make(
        const ConstraintModel& cst) const
{
    if(auto parent_base = dynamic_cast<BaseScenario*>(cst.parent()))
    {
        return DisplayedElementsContainer{
            cst,
            parent_base->startState(),
            parent_base->endState(),

            parent_base->startEvent(),
            parent_base->endEvent(),

            parent_base->startTimeNode(),
            parent_base->endTimeNode()
        };
    }

    return {};
}
