#pragma once
#include <QStringList>
#include <iscore/tools/NamedObject.hpp>
#include <vector>

namespace iscore
{
    class FactoryInterface;
}

class ScenarioContextMenuFactory;

class ScenarioContextMenuPluginList
{
    public:
        const std::vector<ScenarioContextMenuFactory*>& contextMenus() const;

        void registerContextMenu(iscore::FactoryInterface* f);

    private:
        std::vector<ScenarioContextMenuFactory*> m_factories;
};
