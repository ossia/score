#include "ScenarioContextMenuPluginList.hpp"
#include "ScenarioActionsFactory.hpp"

const std::vector<ScenarioActionsFactory *> &ScenarioContextMenuPluginList::contextMenus() const
{
    return m_factories;
}

void ScenarioContextMenuPluginList::registerContextMenu(
        ScenarioActionsFactory *f)
{
    m_factories.push_back(f);
}
