#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

class ScenarioControl;
class AbstractMenuAction;
class ScenarioContextMenuFactory : public iscore::FactoryInterface
{
    public:
        virtual AbstractMenuAction* make(ScenarioControl*) = 0;
};
