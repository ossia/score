#pragma once

#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Tools/dataStructures.hpp>
#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>

class CSPScenario;

class CSPFlexDisplacementPolicy
{
public:

    CSPFlexDisplacementPolicy() = default;

    CSPFlexDisplacementPolicy(Scenario::ScenarioModel& scenario, const QVector<Id<TimeNodeModel>>& draggedElements);

    static
    void
    computeDisplacement(
            Scenario::ScenarioModel& scenario,
            const QVector<Id<TimeNodeModel>>& draggedElements,
            const TimeValue& deltaTime,
            ElementsProperties& elementsProperties);

    static QString name()
    {
        return QString{"CSP"};
    }

    template<typename... Args>
    static void updatePositions(Args&&... args)
    {
        CommonDisplacementPolicy::updatePositions(std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void revertPositions(Args&&... args)
    {
        CommonDisplacementPolicy::revertPositions(std::forward<Args>(args)...);
    }

protected:
    static void refreshStays(CSPScenario& cspScenario, const QVector<Id<TimeNodeModel> >& draggedElements);
};
