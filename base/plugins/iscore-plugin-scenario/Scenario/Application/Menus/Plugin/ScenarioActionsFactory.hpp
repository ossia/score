#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <QList>

#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
class ScenarioActions;
class ScenarioApplicationPlugin;

using ScenarioActionsFactoryKey = UuidKey<ScenarioActions>;
class ISCORE_PLUGIN_SCENARIO_EXPORT ScenarioActionsFactory :
        public iscore::GenericFactoryInterface<ScenarioActionsFactoryKey>
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                ScenarioActions,
                "30a08ebc-bab7-444f-8d8c-860bd7bfe5c7")
    public:
        virtual ~ScenarioActionsFactory() ;
        virtual QList<ScenarioActions*> make(ScenarioApplicationPlugin*) = 0;
};
}

Q_DECLARE_METATYPE(Scenario::ScenarioActionsFactoryKey)
