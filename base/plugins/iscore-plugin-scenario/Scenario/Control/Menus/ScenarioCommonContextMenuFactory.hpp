#pragma once
#include "Plugin/ScenarioActionsFactory.hpp"

class ScenarioCommonActionsFactory : public ScenarioActionsFactory
{
    public:
        QList<ScenarioActions*> make(ScenarioControl* ctrl) override;
};
