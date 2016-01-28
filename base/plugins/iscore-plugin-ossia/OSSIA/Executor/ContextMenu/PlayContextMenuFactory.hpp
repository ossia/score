#pragma once
#include <QList>

#include <Scenario/Application/Menus/Plugin/ScenarioActionsFactory.hpp>

namespace RecreateOnPlay
{
class PlayContextMenuFactory final :
        public Scenario::ScenarioActionsFactory
{
    public:
        const Scenario::ScenarioActionsFactoryKey& concreteFactoryKey() const override;
        QList<Scenario::ScenarioActions*> make(Scenario::ScenarioApplicationPlugin* ctrl) override;
};
}
