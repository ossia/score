#pragma once
#include "Plugin/ScenarioActionsFactory.hpp"

class ScenarioCommonActionsFactory final : public ScenarioActionsFactory
{
    public:
        QList<ScenarioActions*> make(ScenarioControl* ctrl) override;
};
