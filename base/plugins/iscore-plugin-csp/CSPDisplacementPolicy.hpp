#pragma once

#include <Process/ScenarioModel.hpp>
#include <Tools/dataStructures.hpp>
#include <Process/Algorithms/StandardDisplacementPolicy.hpp>

class CSPScenario;

class CSPDisplacementPolicy
{
public:

    CSPDisplacementPolicy() = default;

    CSPDisplacementPolicy(ScenarioModel& scenario, const QVector<Id<TimeNodeModel>>& draggedElements);

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
