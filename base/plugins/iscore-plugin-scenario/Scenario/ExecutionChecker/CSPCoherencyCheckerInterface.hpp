#pragma once

#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Tools/dataStructures.hpp>

namespace Scenario
{
class ISCORE_PLUGIN_SCENARIO_EXPORT CSPCoherencyCheckerInterface
{
    public:
        virtual ~CSPCoherencyCheckerInterface();
        virtual void computeDisplacement(const QVector<Id<Scenario::TimeNodeModel>>& positionnedElements,
                                         Scenario::ElementsProperties& elementsProperties) = 0;
};

}
