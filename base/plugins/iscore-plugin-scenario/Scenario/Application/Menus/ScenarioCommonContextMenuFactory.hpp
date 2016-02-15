#pragma once
#include <QList>

#include "Plugin/ScenarioActionsFactory.hpp"

namespace Scenario
{
class ScenarioActions;
class ScenarioApplicationPlugin;

class ScenarioCommonActionsFactory final : public ScenarioActionsFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("f6ac9feb-4ffd-4fab-93d4-b5834a078c6e")

    public:
        QList<ScenarioActions*> make(ScenarioApplicationPlugin* ctrl) override;
};
}
