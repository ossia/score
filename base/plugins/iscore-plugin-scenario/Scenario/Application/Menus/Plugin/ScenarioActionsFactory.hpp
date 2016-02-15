#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <QList>

#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
class ScenarioActions;
class ScenarioApplicationPlugin;

class ISCORE_PLUGIN_SCENARIO_EXPORT ScenarioActionsFactory :
        public iscore::AbstractFactory<ScenarioActionsFactory>
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                ScenarioActionsFactory,
                "30a08ebc-bab7-444f-8d8c-860bd7bfe5c7")
    public:
        virtual ~ScenarioActionsFactory() ;
        virtual QList<ScenarioActions*> make(ScenarioApplicationPlugin*) = 0;
};
}
