#pragma once

#include <Process/ScenarioModel.hpp>
#include <Tools/dataStructures.hpp>
#include <Process/Algorithms/StandardDisplacementPolicy.hpp>

class CSPScenario;

class CSPFlexDisplacementPolicy
{
public:

    CSPFlexDisplacementPolicy() = default;

    CSPFlexDisplacementPolicy(ScenarioModel& scenario, const QVector<Id<TimeNodeModel>>& draggedElements);

    static
    void
    computeDisplacement(
            ScenarioModel& scenario,
            const QVector<Id<TimeNodeModel>>& draggedElements,
            const TimeValue& deltaTime,
            ElementsProperties& elementsProperties);

    static QString name()
    {
        return QString{"CSP"};
    }

    template<typename ProcessScaleMethod>
    static
    void
    updatePositions(ScenarioModel& scenario, ProcessScaleMethod&& scaleMethod, ElementsProperties& elementsPropertiesToUpdate, bool useNewValues)
    {
        CommonDisplacementPolicy::updatePositions(scenario, scaleMethod, elementsPropertiesToUpdate, useNewValues);
    }
protected:
    static void refreshStays(CSPScenario& cspScenario, const QVector<Id<TimeNodeModel> >& draggedElements);
};
