#pragma once
#include "Plugin/ScenarioActionsFactory.hpp"

class ScenarioCommonActionsFactory final : public ScenarioActionsFactory
{
    public:
        const ScenarioActionsFactoryKey& key_impl() const override;

        QList<ScenarioActions*> make(ScenarioApplicationPlugin* ctrl) override;
};
