#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <QMap>

#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/panel/PanelFactory.hpp>
#include <iscore/command/CommandGeneratorMap.hpp>

namespace iscore
{
class FactoryInterfaceBase;
class Application;
class PluginControlInterface;
class PanelFactory;
class DocumentDelegateFactoryInterface;
class SettingsDelegateFactoryInterface;

using FactoryFamilyList = std::vector<FactoryFamily>;
using CommandList = QList<PluginControlInterface*>;
using PanelList = QList<PanelFactory*>;
using DocumentPanelList = QList<DocumentDelegateFactoryInterface*>;
using SettingsList = QList<SettingsDelegateFactoryInterface*>;

/**
     * @brief The PluginManager class loads and keeps track of the plug-ins.
     */
class PluginManager final : public QObject
{
        Q_OBJECT
        friend class iscore::Application;
    public:
        PluginManager(iscore::Application* app);
        ~PluginManager();

        /**
             * @brief reloadPlugins
             *
             * Reloads all the plug-ins.
             * Note: for now this is unsafe after the first loading.
             */
        void reloadPlugins();

        /**
             * @brief pluginsOnSystem
             * @return All the plugins available on the system
             *
             * Even plug-ins that were not loaded will be returned.
             */
        QStringList pluginsOnSystem() const;

        void addControl(PluginControlInterface* p)
        { m_controlList.push_back(p); }

        void addPanel(PanelFactory* p)
        { m_panelList.push_back(p); }

    private:
        iscore::Application* m_app{};

        // We need a list for all the plug-ins present on the system even if we do not load them.
        // Else we can't blacklist / unblacklist plug-ins.
        QStringList m_pluginsOnSystem;

        void loadPlugin(const QString& filename);

        void loadControls(QObject* plugin);
        void loadFactories(QObject* plugin);

        // Classify the plug-in element in the correct container.
        void dispatch(QObject* plugin);

        void clearPlugins();

        QStringList pluginsBlacklist();

        // Here, the plug-ins that are effectively loaded.
        QList<QObject*> m_availablePlugins;

        FactoryFamilyList m_customFamilies;
        CommandList  m_controlList;
        PanelList    m_panelList;
        DocumentPanelList m_documentPanelList;
        SettingsList m_settingsList;

        std::vector<std::vector<iscore::FactoryInterfaceBase*>> m_factoriesInterfaces;

        std::unordered_map<CommandParentFactoryKey, CommandGeneratorMap> m_commands;
};
}
