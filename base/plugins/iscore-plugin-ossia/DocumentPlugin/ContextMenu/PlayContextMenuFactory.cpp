#include "PlayContextMenuFactory.hpp"
#include "PlayContextMenu.hpp"
const ScenarioActionsFactoryKey&PlayContextMenuFactory::key_impl() const
{
    static const ScenarioActionsFactoryKey fact{"PlayContextMenuFactory"};
    return fact;
}

QList<ScenarioActions *> PlayContextMenuFactory::make(ScenarioApplicationPlugin *ctrl)
{
    return {new PlayContextMenu(ctrl)};
}
