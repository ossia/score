#pragma once
#include "Scenario/Control/Menus/Plugin/ScenarioActionsFactory.hpp"

class PlayContextMenuFactory final : public ScenarioActionsFactory
{
    public:
        QList<ScenarioActions*> make(ScenarioControl* ctrl) override;
};
