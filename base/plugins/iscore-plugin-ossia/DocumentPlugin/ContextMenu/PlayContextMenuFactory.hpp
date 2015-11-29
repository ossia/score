#pragma once
#include <QList>

#include "Scenario/Application/Menus/Plugin/ScenarioActionsFactory.hpp"

class ScenarioActions;
class ScenarioApplicationPlugin;

class PlayContextMenuFactory final : public ScenarioActionsFactory
{
    public:
        const ScenarioActionsFactoryKey& key_impl() const override;
        QList<ScenarioActions*> make(ScenarioApplicationPlugin* ctrl) override;
};
