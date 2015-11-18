#pragma once
#include "Scenario/Control/Menus/Plugin/ScenarioActionsFactory.hpp"

class PlayContextMenuFactory final : public ScenarioActionsFactory
{
    public:
        const ScenarioActionsFactoryKey& key_impl() const override;
        QList<ScenarioActions*> make(ScenarioControl* ctrl) override;
};
