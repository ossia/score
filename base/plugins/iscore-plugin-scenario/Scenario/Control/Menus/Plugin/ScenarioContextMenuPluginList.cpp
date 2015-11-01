#include "ScenarioContextMenuPluginList.hpp"
#include "ScenarioActionsFactory.hpp"

const std::vector<ScenarioActionsFactory *> &ScenarioContextMenuPluginList::contextMenus() const
{
    return m_factories;
}

void ScenarioContextMenuPluginList::registerContextMenu(
        iscore::FactoryInterface *f)
{
    m_factories.push_back(static_cast<ScenarioActionsFactory*>(f));
}
