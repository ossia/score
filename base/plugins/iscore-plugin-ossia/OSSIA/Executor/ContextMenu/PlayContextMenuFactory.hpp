#pragma once
#include <QList>

#include <Scenario/Application/Menus/Plugin/ScenarioActionsFactory.hpp>

namespace RecreateOnPlay
{
class PlayContextMenuFactory final :
        public Scenario::ScenarioActionsFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("c5bb64b3-6856-4479-912f-040d4ae78be3")
    public:
        QList<Scenario::ScenarioActions*> make(Scenario::ScenarioApplicationPlugin* ctrl) override;
};
}
