#include "PlayContextMenu.hpp"
#include "PlayContextMenuFactory.hpp"

namespace RecreateOnPlay
{
const Scenario::ScenarioActionsFactoryKey& PlayContextMenuFactory::concreteFactoryKey() const
{
    static const Scenario::ScenarioActionsFactoryKey fact{"c5bb64b3-6856-4479-912f-040d4ae78be3"};
    return fact;
}

QList<Scenario::ScenarioActions *> PlayContextMenuFactory::make(
        Scenario::ScenarioApplicationPlugin *ctrl)
{
    return {new PlayContextMenu(ctrl)};
}
}
