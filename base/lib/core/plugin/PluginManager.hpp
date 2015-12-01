#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <vector>

namespace iscore
{
class Application;
class ApplicationRegistrar;

class PluginLoader
{
        friend class iscore::Application;
    public:
        PluginLoader(iscore::Application* app);

        /**
             * @brief loadPlugins
             *
             * Reloads all the plug-ins.
             * Note: for now this is unsafe after the first loading.
             */
        void loadPlugins(iscore::ApplicationRegistrar&);

    private:
        iscore::Application* m_app{};

        // We need a list for all the plug-ins present on the system even if we do not load them.
        // Else we can't blacklist / unblacklist plug-ins.
        QStringList m_pluginsOnSystem;

        // QString is set if it's a valid plug-in file,
        // QObject* is set if the plug-in could be loaded
        std::pair<QString, QObject*> loadPlugin(
                const QString& filename,
                const std::vector<QObject*>& availablePlugins);

        QStringList pluginsBlacklist();
};
}
