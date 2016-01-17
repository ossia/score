#include <boost/concept/usage.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <iscore/application/ApplicationComponents.hpp>
#include <core/application/ApplicationRegistrar.hpp>
#include <core/plugin/PluginManager.hpp>
#include <iscore/plugins/qt_interfaces/DocumentDelegateFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/SettingsDelegateFactoryInterface_QtInterface.hpp>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include <QPluginLoader>
#include <QSettings>
#include <QVariant>
#include <unordered_map>
#include <utility>

#include "PluginDependencyGraph.hpp"
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/tools/std/Algorithms.hpp>


namespace iscore
{
/**
 * @brief pluginsDir
 * @return The folder where i-score should look for plug-ins, static for now.
 */
static QStringList pluginsDir()
{
#if defined(_WIN32)
    return {QCoreApplication::applicationDirPath() + "/plugins"};
#elif defined(__linux__)
    return {QCoreApplication::applicationDirPath() + "/plugins",
            "/usr/lib/i-score/plugins"};
#elif defined(__APPLE__) && defined(__MACH__)
    return {QCoreApplication::applicationDirPath() + "/plugins",
            QCoreApplication::applicationDirPath() + "../Frameworks/i-score/plugins"};
#else
    return {};
#endif
}

ISCORE_LIB_BASE_EXPORT
std::pair<QString, QObject*> PluginLoader::loadPlugin(
        const QString &fileName,
        const std::vector<QObject*>& availablePlugins)
{
#if !defined(ISCORE_STATIC_QT)
    auto blacklist = pluginsBlacklist();
    QPluginLoader loader {fileName};

    if(QObject* plugin = loader.instance())
    {
        // Check if the plugin is not already loaded
        if(find_if(availablePlugins,
           [&] (QObject* obj)
           { return obj->metaObject()->className() == plugin->metaObject()->className(); })
                != availablePlugins.end())
        {
            qDebug() << "Warning: plugin"
                     << plugin->metaObject()->className()
                     << "was already loaded. Not reloading.";

            return std::make_pair(fileName, nullptr);
        }

        // Check if it is blacklisted
        if(!blacklist.contains(fileName))
        {
            return std::make_pair(fileName, plugin);
        }
        else
        {
            plugin->deleteLater();
            return std::make_pair(fileName, nullptr);
        }

    }
    else
    {
        QString s = loader.errorString();
        if(!s.contains("Plugin verification data mismatch") && !s.contains("is not a Qt plugin"))
            qDebug() << "Error while loading" << fileName << ": " << loader.errorString();
        return {};
    }
#endif

    return {};
}

void PluginLoader::loadPlugins(
        iscore::ApplicationRegistrar& registrar,
        const iscore::ApplicationContext& context)
{
    auto folders = pluginsDir();

    // Here, the plug-ins that are effectively loaded.
    std::vector<QObject*> availablePlugins;

    // Load static plug-ins
    for(QObject* plugin : QPluginLoader::staticInstances())
    {
        availablePlugins.push_back(plugin);
    }

    QSet<QString> pluginFiles;
#if !defined(ISCORE_STATIC_QT)
    // Load dynamic plug-ins
    for(const QString& pluginsFolder : folders)
    {
        QDir pluginsDir(pluginsFolder);
        for(const QString& fileName : pluginsDir.entryList(QDir::Files))
        {
            auto plug = loadPlugin(pluginsDir.absoluteFilePath(fileName), availablePlugins);

            if(!plug.first.isEmpty())
            {
                pluginFiles.insert(plug.first);
            }

            if(plug.second)
            {
                availablePlugins.push_back(plug.second);
            }
        }
    }
#endif

    // First bring in the plugin objects
    registrar.registerPlugins(
                pluginFiles.toList(),
                availablePlugins);


    // Here, it is important not to collapse all the for-loops
    // because for instance a ApplicationPlugin from plugin B might require the factory
    // from plugin A to be loaded prior.
    // Load all the factories.
    for(QObject* plugin : availablePlugins)
    {
        auto facfam_interface = qobject_cast<FactoryList_QtInterface*> (plugin);

        if(facfam_interface)
        {
            for(auto&& elt : facfam_interface->factoryFamilies())
            {
                registrar.registerFactory(std::move(elt));
            }
        }
    }

    // Load all the application context plugins.
    // We have to order them according to their dependencies
    PluginDependencyGraph graph;
    for(QObject* plugin : availablePlugins)
    {
        graph.addNode(plugin);
    }

    graph.visit([&] (QObject* plugin) {
        auto ctrl_plugin = qobject_cast<GUIApplicationContextPlugin_QtInterface*> (plugin);
        if(ctrl_plugin)
        {
            auto plug = ctrl_plugin->make_applicationPlugin(context);
            registrar.registerApplicationContextPlugin(plug);
        }
    });

    // Load what the plug-ins have to offer.
    for(QObject* plugin : availablePlugins)
    {
        auto settings_plugin = qobject_cast<SettingsDelegateFactoryInterface_QtInterface*> (plugin);
        if(settings_plugin)
        {// TODO change the name in the correct order.
            registrar.registerSettings(settings_plugin->settings_make());
        }

        auto panel_plugin = qobject_cast<PanelFactory_QtInterface*> (plugin);
        if(panel_plugin)
        {
            auto panels = panel_plugin->panels();
            for(auto panel : panels)
            {
                registrar.registerPanel(panel);
            }
        }

        auto docpanel_plugin = qobject_cast<DocumentDelegateFactoryInterface_QtInterface*> (plugin);
        if(docpanel_plugin)
        {
            auto docs = docpanel_plugin->documents();
            for(auto doc_del : docs)
            {
                registrar.registerDocumentDelegate(doc_del);
            }
        }

        auto commands_plugin = qobject_cast<CommandFactory_QtInterface*> (plugin);
        if(commands_plugin)
        {
            registrar.registerCommands(commands_plugin->make_commands());
        }

        auto factories_plugin = qobject_cast<FactoryInterface_QtInterface*> (plugin);
        if(factories_plugin)
        {
            for(auto& factory_family : registrar.components().factories)
            {
                for(auto&& new_factory : factories_plugin->factories(context, factory_family.first))
                {
                    factory_family.second->insert(std::move(new_factory));
                }
            }
        }
    }
}

QStringList PluginLoader::pluginsBlacklist()
{
    QSettings s;
    return s.value("PluginSettings/Blacklist", QStringList {}).toStringList();
}
}

