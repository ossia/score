#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <boost/optional/optional.hpp>
#include <qobject.h>

#include "Scenario/Document/Event/EventModel.hpp"
#include "Scenario/Document/State/StateModel.hpp"
#include "Scenario/Document/TimeNode/TimeNodeModel.hpp"
#include "ScenarioDisplayedElementsProvider.hpp"
#include "iscore/tools/NotifyingMap.hpp"
#include "iscore/tools/SettableIdentifier.hpp"

bool ScenarioDisplayedElementsProvider::matches(
        const ConstraintModel& cst) const
{
    return dynamic_cast<Scenario::ScenarioModel*>(cst.parent());
}

DisplayedElementsContainer ScenarioDisplayedElementsProvider::make(
        const ConstraintModel& cst) const
{
    if(auto parent_scenario = dynamic_cast<Scenario::ScenarioModel*>(cst.parent()))
    {
        const auto& sst = parent_scenario->states.at(cst.startState());
        const auto& est = parent_scenario->states.at(cst.endState());
        const auto& sev = parent_scenario->events.at(sst.eventId());
        const auto& eev = parent_scenario->events.at(est.eventId());
        return DisplayedElementsContainer{
            cst,
            sst,
            est,
            sev,
            eev,
            parent_scenario->timeNodes.at(sev.timeNode()),
            parent_scenario->timeNodes.at(eev.timeNode())
        };
    }

    return {};
}
