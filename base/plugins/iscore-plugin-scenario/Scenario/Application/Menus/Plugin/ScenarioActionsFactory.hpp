#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <QList>

#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
class ScenarioActions;
class ScenarioApplicationPlugin;

class ScenarioActionsTag{};
using ScenarioActionsFactoryKey = StringKey<ScenarioActionsTag>;
class ISCORE_PLUGIN_SCENARIO_EXPORT ScenarioActionsFactory :
        public iscore::GenericFactoryInterface<ScenarioActionsFactoryKey>
{
        ISCORE_FACTORY_DECL("ScenarioContextMenu")
    public:
            using factory_key_type = ScenarioActionsFactoryKey;
        virtual ~ScenarioActionsFactory() ;
        virtual QList<ScenarioActions*> make(ScenarioApplicationPlugin*) = 0;
};
}

Q_DECLARE_METATYPE(Scenario::ScenarioActionsFactoryKey)
