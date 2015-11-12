#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <QList>

class ScenarioControl;
class ScenarioActions;

class ScenarioActionsFactory : public iscore::FactoryInterfaceBase
{
        ISCORE_FACTORY_DECL("ScenarioContextMenu")
    public:
        virtual QList<ScenarioActions*> make(ScenarioControl*) = 0;
};

