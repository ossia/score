#include "PlayContextMenuFactory.hpp"
#include "PlayContextMenu.hpp"
QList<ScenarioActions *> PlayContextMenuFactory::make(ScenarioControl *ctrl)
{
    return {new PlayContextMenu(ctrl)};
}
