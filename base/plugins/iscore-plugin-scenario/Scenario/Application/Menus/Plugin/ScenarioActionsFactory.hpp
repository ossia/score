#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <QList>

#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore_plugin_scenario_export.h>
class ScenarioActions;
class ScenarioApplicationPlugin;

class ScenarioActionsTag{};
using ScenarioActionsFactoryKey = StringKey<ScenarioActionsTag>;
Q_DECLARE_METATYPE(ScenarioActionsFactoryKey)

class ISCORE_PLUGIN_SCENARIO_EXPORT ScenarioActionsFactory :
        public iscore::GenericFactoryInterface<ScenarioActionsFactoryKey>
{
        ISCORE_FACTORY_DECL("ScenarioContextMenu")
    public:
            using factory_key_type = ScenarioActionsFactoryKey;
        virtual ~ScenarioActionsFactory() ;
        virtual QList<ScenarioActions*> make(ScenarioApplicationPlugin*) = 0;
};

