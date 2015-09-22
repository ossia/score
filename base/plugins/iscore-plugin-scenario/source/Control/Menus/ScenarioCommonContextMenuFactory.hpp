#pragma once
#include "Plugin/ScenarioContextMenuFactoryFamily.hpp"

class ScenarioCommonActionsFactory : public ScenarioActionsFactory
{
    public:
        QList<ScenarioActions*> make(ScenarioControl* ctrl) override;
};
