#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

class ScenarioControl;
class AbstractMenuActions;
class ScenarioContextMenuFactory : public iscore::FactoryInterface
{
    public:
        static QString factoryName() { return "ScenarioContextMenu"; }
        virtual QList<AbstractMenuActions*> make(ScenarioControl*) = 0;
};

