#include "PlayContextMenu.hpp"
#include "PlayContextMenuFactory.hpp"

namespace RecreateOnPlay
{
QList<Scenario::ScenarioActions *> PlayContextMenuFactory::make(
        Scenario::ScenarioApplicationPlugin *ctrl)
{
    return {new PlayContextMenu(ctrl)};
}
}
