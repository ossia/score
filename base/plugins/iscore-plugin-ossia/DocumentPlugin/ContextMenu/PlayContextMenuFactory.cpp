#include "PlayContextMenuFactory.hpp"
#include "PlayContextMenu.hpp"
QList<AbstractMenuActions *> PlayContextMenuFactory::make(ScenarioControl *ctrl)
{
    return {new PlayContextMenu(ctrl)};
}
