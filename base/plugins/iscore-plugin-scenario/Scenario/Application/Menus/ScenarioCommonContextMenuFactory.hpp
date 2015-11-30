#pragma once
#include <QList>

#include "Plugin/ScenarioActionsFactory.hpp"

class ScenarioActions;
class ScenarioApplicationPlugin;

class ScenarioCommonActionsFactory final : public ScenarioActionsFactory
{
    public:
        const ScenarioActionsFactoryKey& key_impl() const override;

        QList<ScenarioActions*> make(ScenarioApplicationPlugin* ctrl) override;
};
