#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <QList>

class ScenarioApplicationPlugin;
class ScenarioActions;

class ScenarioActionsTag{};
using ScenarioActionsFactoryKey = StringKey<ScenarioActionsTag>;
Q_DECLARE_METATYPE(ScenarioActionsFactoryKey)

class ScenarioActionsFactory :
        public iscore::GenericFactoryInterface<ScenarioActionsFactoryKey>
{
        ISCORE_FACTORY_DECL("ScenarioContextMenu")
    public:
            using factory_key_type = ScenarioActionsFactoryKey;
        virtual ~ScenarioActionsFactory() ;
        virtual QList<ScenarioActions*> make(ScenarioApplicationPlugin*) = 0;
};

