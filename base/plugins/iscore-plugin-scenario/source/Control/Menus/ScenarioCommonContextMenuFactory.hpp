#pragma once
#include "Plugin/ScenarioContextMenuFactoryFamily.hpp"

class ScenarioCommonContextMenuFactory : public ScenarioContextMenuFactory
{
    public:
        QList<AbstractMenuActions*> make(ScenarioControl* ctrl) override;
};
