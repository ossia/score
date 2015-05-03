#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <QPluginLoader>
#include <QMap>

#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/panel/PanelFactory.hpp>

namespace iscore
{
    class PluginControlInterface;
    class PanelFactory;
    class DocumentDelegateFactoryInterface;
    class SettingsDelegateFactoryInterface;

    using FactoryFamilyList = QVector<FactoryFamily>;
    using CommandList = QList<PluginControlInterface*>;
    using PanelList = QList<PanelFactory*>;
    using DocumentPanelList = QList<DocumentDelegateFactoryInterface*>;
    using SettingsList = QList<SettingsDelegateFactoryInterface*>;

    /**
     * @brief The PluginManager class loads and keeps track of the plug-ins.
     */
    class PluginManager : public NamedObject
    {
            Q_OBJECT
            friend class Application;
        public:
            PluginManager(QObject* parent) :
                NamedObject {"PluginManager", parent}
            {
            }

            ~PluginManager()
            {
                clearPlugins();
            }

            void reloadPlugins();

            QStringList pluginsOnSystem() const
            {
                return m_pluginsOnSystem;
            }

            void addControl(PluginControlInterface* p)
            { m_commandList.push_back(p); }

            void addPanel(PanelFactory* p)
            { m_panelList.push_back(p); }

        private:
            // We need a list for all the plug-ins present on the system even if we do not load them.
            // Else we can't blacklist / unblacklist plug-ins.
            QStringList m_pluginsOnSystem;

            void loadFactories(QObject* plugin);
            void dispatch(QObject* plugin);
            void clearPlugins();

            QStringList pluginsBlacklist();

            // Here, the plug-ins that are effectively loaded.
            QMap<QString, QObject*> m_availablePlugins;

            FactoryFamilyList m_customFamilies;
            CommandList  m_commandList;
            PanelList    m_panelList;
            DocumentPanelList m_documentPanelList;
            SettingsList m_settingsList;

            QVector<QVector<FactoryInterface*>> m_factoriesInterfaces;
    };
}
