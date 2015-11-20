#pragma once
#include "Scenario/Application/Menus/Plugin/ScenarioActionsFactory.hpp"

class PlayContextMenuFactory final : public ScenarioActionsFactory
{
    public:
        const ScenarioActionsFactoryKey& key_impl() const override;
        QList<ScenarioActions*> make(ScenarioApplicationPlugin* ctrl) override;
};
