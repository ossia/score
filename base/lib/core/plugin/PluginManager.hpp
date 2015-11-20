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
class ApplicationRegistrar;
class GUIApplicationContextPlugin;
class PanelFactory;
class DocumentDelegateFactoryInterface;
class SettingsDelegateFactoryInterface;


/**
     * @brief The PluginManager class loads and keeps track of the plug-ins.
     */
class PluginLoader final : public QObject
{
        Q_OBJECT
        friend class iscore::Application;
    public:
        PluginLoader(iscore::Application* app);
        ~PluginLoader();

        /**
             * @brief reloadPlugins
             *
             * Reloads all the plug-ins.
             * Note: for now this is unsafe after the first loading.
             */
        void clearPlugins();
        void reloadPlugins(iscore::ApplicationRegistrar&);

        /**
             * @brief pluginsOnSystem
             * @return All the plugins available on the system
             *
             * Even plug-ins that were not loaded will be returned.
             */
        QStringList pluginsOnSystem() const;

    private:
        iscore::Application* m_app{};

        // We need a list for all the plug-ins present on the system even if we do not load them.
        // Else we can't blacklist / unblacklist plug-ins.
        QStringList m_pluginsOnSystem;

        void loadPlugin(const QString& filename);


        // Classify the plug-in element in the correct container.
        void dispatch(QObject* plugin);


        QStringList pluginsBlacklist();

        // Here, the plug-ins that are effectively loaded.
        std::vector<QObject*> m_availablePlugins;
};
}
