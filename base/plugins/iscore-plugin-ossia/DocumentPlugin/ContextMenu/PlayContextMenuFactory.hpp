#pragma once
#include "source/Control/Menus/Plugin/ScenarioContextMenuFactoryFamily.hpp"

class PlayContextMenuFactory : public ScenarioContextMenuFactory
{
    public:
        QList<AbstractMenuActions*> make(ScenarioControl* ctrl) override;
};
