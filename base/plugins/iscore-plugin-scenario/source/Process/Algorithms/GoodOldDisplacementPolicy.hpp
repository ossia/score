#pragma once

#include <QVector>
#include <iscore/tools/SettableIdentifier.hpp>
#include <Process/Algorithms/StandardDisplacementPolicy.hpp>

class QString;
class ScenarioModel;
class TimeNodeModel;
struct ElementsProperties;

class GoodOldDisplacementPolicy
{
public:

    GoodOldDisplacementPolicy() = default;

    GoodOldDisplacementPolicy(ScenarioModel& scenario, const QVector<Id<TimeNodeModel>>& draggedElements){}

    static
    void
    computeDisplacement(
            ScenarioModel& scenario,
            const QVector<Id<TimeNodeModel>>& draggedElements,
            const TimeValue& deltaTime,
            ElementsProperties& elementsProperties);

    static QString name()
    {
        return QString{"Old way"};
    }

    template<typename ProcessScaleMethod>
    static
    void
    updatePositions(ScenarioModel& scenario, ProcessScaleMethod&& scaleMethod, ElementsProperties& elementsPropertiesToUpdate, bool useNewValues)
    {
        CommonDisplacementPolicy::updatePositions(scenario, scaleMethod, elementsPropertiesToUpdate, useNewValues);
    }
};
