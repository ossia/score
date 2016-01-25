#include "PlayContextMenu.hpp"
#include "PlayContextMenuFactory.hpp"

namespace RecreateOnPlay
{
const Scenario::ScenarioActionsFactoryKey& PlayContextMenuFactory::concreteFactoryKey() const
{
    static const Scenario::ScenarioActionsFactoryKey fact{"PlayContextMenuFactory"};
    return fact;
}

QList<Scenario::ScenarioActions *> PlayContextMenuFactory::make(
        Scenario::ScenarioApplicationPlugin *ctrl)
{
    return {new PlayContextMenu(ctrl)};
}
}
