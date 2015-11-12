#pragma once
#include <QStringList>
#include <iscore/tools/NamedObject.hpp>
#include <vector>

class ScenarioActionsFactory;

class ScenarioContextMenuPluginList
{
    public:
        const std::vector<ScenarioActionsFactory*>& contextMenus() const;

        void registerContextMenu(ScenarioActionsFactory* f);

    private:
        std::vector<ScenarioActionsFactory*> m_factories;
};
