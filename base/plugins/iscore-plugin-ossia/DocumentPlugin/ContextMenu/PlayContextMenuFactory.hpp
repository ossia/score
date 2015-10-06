#pragma once
#include "source/Control/Menus/Plugin/ScenarioActionsFactory.hpp"

class PlayContextMenuFactory : public ScenarioActionsFactory
{
    public:
        QList<ScenarioActions*> make(ScenarioControl* ctrl) override;
};
