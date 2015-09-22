#include "ScenarioContextMenuPluginList.hpp"
#include "ScenarioContextMenuFactoryFamily.hpp"

const std::vector<ScenarioActionsFactory *> &ScenarioContextMenuPluginList::contextMenus() const
{
    return m_factories;
}

void ScenarioContextMenuPluginList::registerContextMenu(
        iscore::FactoryInterface *f)
{
    m_factories.push_back(static_cast<ScenarioActionsFactory*>(f));
}
