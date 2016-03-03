#pragma once

#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore_plugin_scenario_export.h>

#include "CSPCoherencyCheckerInterface.hpp"

namespace Scenario
{
class ISCORE_PLUGIN_SCENARIO_EXPORT CoherencyCheckerFactoryInterface :
         public iscore::AbstractFactory<CoherencyCheckerFactoryInterface>
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                CoherencyCheckerFactoryInterface,
                "e9942ad6-1e39-4bdf-bb93-f31962e3cf79")

    public:
        virtual CSPCoherencyCheckerInterface* make(
                    Scenario::ScenarioModel& scenario,
                    Scenario::ElementsProperties& elementsProperties) = 0;
        virtual ~CoherencyCheckerFactoryInterface();
};

}
