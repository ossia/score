#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

class ScenarioControl;
class ScenarioActions;

class ScenarioActionsFactory : public iscore::FactoryInterface
{
    public:
        static QString factoryName() { return "ScenarioContextMenu"; }
        virtual QList<ScenarioActions*> make(ScenarioControl*) = 0;
};

