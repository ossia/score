#pragma once

#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>
#include <QString>
#include <QVector>
#include <algorithm>
#include <vector>

#include <Process/TimeValue.hpp>

class TimeNodeModel;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario
struct ElementsProperties;
#include <iscore/tools/SettableIdentifier.hpp>

class GoodOldDisplacementPolicy
{
public:

    GoodOldDisplacementPolicy() = default;

    GoodOldDisplacementPolicy(Scenario::ScenarioModel& scenario, const QVector<Id<TimeNodeModel>>& draggedElements){}

    static void computeDisplacement(
            Scenario::ScenarioModel& scenario,
            const QVector<Id<TimeNodeModel>>& draggedElements,
            const TimeValue& deltaTime,
            ElementsProperties& elementsProperties);

    static void getRelatedTimeNodes(
            Scenario::ScenarioModel& scenario,
            const Id<TimeNodeModel>& firstTimeNodeMovedId,
            std::vector<Id<TimeNodeModel> >& translatedTimeNodes);

    static QString name()
    {
        return QString{"Old way"};
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
};
