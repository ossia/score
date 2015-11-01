#pragma once
#include <QStringList>
#include <iscore/tools/NamedObject.hpp>
#include <vector>

namespace iscore
{
    class FactoryInterface;
}

class ScenarioActionsFactory;

class ScenarioContextMenuPluginList
{
    public:
        const std::vector<ScenarioActionsFactory*>& contextMenus() const;

        void registerContextMenu(iscore::FactoryInterface* f);

    private:
        std::vector<ScenarioActionsFactory*> m_factories;
};
