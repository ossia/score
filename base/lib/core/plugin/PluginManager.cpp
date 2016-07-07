#include <boost/concept/usage.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <iscore/application/GUIApplicationContext.hpp>
#include <iscore/application/ApplicationComponents.hpp>
#include <core/application/ApplicationRegistrar.hpp>
#include <core/plugin/PluginManager.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include <QJsonDocument>
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
#include <QStandardPaths>

namespace iscore
{

/**
 * @brief pluginsDir
 * @return The folder where i-score should look for plug-ins, static for now.
 */
static QStringList pluginsDir()
{
    QStringList l;
#if defined(_WIN32)
    l << (QCoreApplication::applicationDirPath() + "/plugins");
#elif defined(__linux__)
    l << QCoreApplication::applicationDirPath() + "/plugins"
      << QCoreApplication::applicationDirPath() + "/../lib/i-score"
      << "/usr/lib/i-score";
#elif defined(__APPLE__) && defined(__MACH__)
    l << QCoreApplication::applicationDirPath() + "/plugins"
      << QCoreApplication::applicationDirPath() + "../Frameworks/i-score/plugins";
#endif

    qDebug() << l;
    return l;
}


static QStringList addonsDir()
{
    QStringList l;

#if !defined(ISCORE_DEPLOYMENT_BUILD)
    l << "addons";
#endif
    l << QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first() + "/i-score/addons";
    qDebug() << l;
    return l;
}
enum class PluginLoadingError
{
    NoError, Blacklisted, NotAPlugin, AlreadyLoaded, UnknownError
};

static
std::pair<iscore::Plugin_QtInterface*, PluginLoadingError> loadPlugin(
        const QString &fileName,
        const std::vector<iscore::Addon>& availablePlugins)
{
    using namespace iscore::PluginLoader;
#if !defined(ISCORE_STATIC_QT)
    auto blacklist = pluginsBlacklist();

    // Check if it is blacklisted
    if(blacklist.contains(fileName))
    {
        return std::make_pair(nullptr, PluginLoadingError::Blacklisted);
    }
    
#if defined(_MSC_VER)
    QDir iscore_dir = QDir::current();
    QPluginLoader loader {iscore_dir.relativeFilePath(fileName)};
#else 
    QPluginLoader loader {fileName};
#endif

    if(QObject* plugin = loader.instance())
    {
        auto iscore_plugin = dynamic_cast<iscore::Plugin_QtInterface*>(plugin);
        if(!iscore_plugin)
        {
            qDebug() << "Warning: plugin"
                     << plugin->metaObject()->className()
                     << "is not an i-score plug-in.";
            return std::make_pair(nullptr, PluginLoadingError::NotAPlugin);
        }

        // Check if the plugin is not already loaded
        auto plug_it =
                find_if(availablePlugins,
                        [&] (const iscore::Addon& obj)
        { return obj.plugin && (obj.plugin->key() == iscore_plugin->key()); });

        if(plug_it != availablePlugins.end())
        {
            qDebug() << "Warning: plugin"
                     << plugin->metaObject()->className()
                     << "was already loaded. Not reloading.";

            return std::make_pair(nullptr, PluginLoadingError::AlreadyLoaded);
        }

        // We can load the plug-in
        return std::make_pair(iscore_plugin, PluginLoadingError::NoError);
    }
    else
    {
        QString s = loader.errorString();
        if(!s.contains("Plugin verification data mismatch") && !s.contains("is not a Qt plugin"))
            qDebug() << "Error while loading" << fileName << ": " << loader.errorString();
        return std::make_pair(nullptr, PluginLoadingError::UnknownError);
    }
#endif

    return std::make_pair(nullptr, PluginLoadingError::UnknownError);
}

static void loadPluginsInAllFolders(std::vector<iscore::Addon>& availablePlugins)
{
    using namespace iscore::PluginLoader;

#if !defined(ISCORE_STATIC_QT)
    // Load dynamic plug-ins
    for(const QString& pluginsFolder : pluginsDir())
    {
        QDir pluginsDir(pluginsFolder);
        for(const QString& fileName : pluginsDir.entryList(QDir::Files))
        {
            auto path = pluginsDir.absoluteFilePath(fileName);
            auto plug = loadPlugin(path, availablePlugins);

            switch(plug.second)
            {
                case PluginLoadingError::NoError:
                {
                    iscore::Addon addon;
                    addon.path = path;
                    addon.plugin = plug.first;
                    addon.corePlugin = true;
                    availablePlugins.push_back(std::move(addon));
                    break;
                }
                default:
                    break;
            }
        }
    }
#endif
}

static optional<iscore::Addon> makeAddon(
        const QString& addon_path,
        const QJsonObject& json_addon,
        const std::vector<iscore::Addon>& availablePlugins)
{
    using namespace iscore::PluginLoader;
    using Funmap = std::map<QString, std::function<void(QJsonValue)>>;

    iscore::Addon add;

    const Funmap funmap
    {
        { addonArchitecture(), [&] (QJsonValue v) {
                QString path = addon_path + "/" + v.toString();
                auto plug = loadPlugin(path, availablePlugins);

                switch(plug.second)
                {
                    case PluginLoadingError::NoError:
                    {
                        add.path = path;
                        add.plugin = plug.first;
                        break;
                    }
                    case PluginLoadingError::Blacklisted:
                    {
                        add.path = path;
                        add.enabled = false;
                        break;
                    }
                    default:
                        break;
                }
            }
        },
        { "name",    [&] (QJsonValue v) { add.name = v.toString(); } },
        { "version", [&] (QJsonValue v) { add.version = v.toString(); } },
        { "url",     [&] (QJsonValue v) { add.latestVersionAddress = v.toString(); } },
        { "short",   [&] (QJsonValue v) { add.shortDescription = v.toString(); } },
        { "long",    [&] (QJsonValue v) { add.longDescription = v.toString(); } },
        { "key",     [&] (QJsonValue v) { add.key = UuidKey<Addon>::fromString(v.toString()); } },
        { "small",   [&] (QJsonValue v) { add.smallImage = QImage{v.toString()}; } },
        { "large",   [&] (QJsonValue v) { add.largeImage = QImage{v.toString()}; } }
    };

    for(auto k : json_addon.keys())
    {
        auto fun = funmap.find(k);
        if(fun != funmap.end())
        {
            fun->second(json_addon[k]);
        }
    }

    if(add.name.isEmpty() || add.path.isEmpty() || add.key.impl().is_nil())
        return iscore::none;

    return add;
}

static void loadAddonsInAllFolders(std::vector<iscore::Addon>& availablePlugins)
{
    using namespace iscore::PluginLoader;

    // Load dynamic plug-ins
    for(const QString& pluginsFolder : addonsDir())
    {
        QDir pluginsDir{pluginsFolder};
        for(const QString& dirName : pluginsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        {
            auto folder = pluginsFolder + "/" + dirName;
            QFile addonFile{folder + "/localaddon.json"};

            // First look for a addon.json file
            if(!addonFile.exists())
                continue;
            addonFile.open(QFile::ReadOnly);

            auto addon = makeAddon(
                        folder,
                        QJsonDocument::fromJson(addonFile.readAll()).object(),
                        availablePlugins);

            if(addon)
                availablePlugins.push_back(std::move(*addon));
        }
    }
}

void PluginLoader::loadPlugins(
        iscore::ApplicationRegistrar& registrar,
        const iscore::ApplicationContext& context)
{
    // Here, the plug-ins that are effectively loaded.
    std::vector<iscore::Addon> availablePlugins;

    // Load static plug-ins
    for(QObject* plugin : QPluginLoader::staticInstances())
    {
        if(auto iscore_plug = dynamic_cast<iscore::Plugin_QtInterface*>(plugin))
        {
            iscore::Addon addon;
            addon.corePlugin = true;
            addon.plugin = iscore_plug;
            availablePlugins.push_back(std::move(addon));
        }
    }

    loadPluginsInAllFolders(availablePlugins);
    loadAddonsInAllFolders(availablePlugins);

    // First bring in the plugin objects
    registrar.registerAddons(
                availablePlugins);

    // Here, it is important not to collapse all the for-loops
    // because for instance a ApplicationPlugin from plugin B might require the factory
    // from plugin A to be loaded prior.
    // Load all the factories.
    for(const iscore::Addon& addon : availablePlugins)
    {
        auto facfam_interface = dynamic_cast<FactoryList_QtInterface*> (addon.plugin);

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
    for(const iscore::Addon& addon : availablePlugins)
    {
        graph.addNode(dynamic_cast<QObject*>(addon.plugin));
    }

    if(auto gui_ctx = dynamic_cast<const iscore::GUIApplicationContext*>(&context))
    {
        graph.visit([&] (QObject* plugin) {
            auto ctrl_plugin = dynamic_cast<GUIApplicationContextPlugin_QtInterface*> (plugin);
            if(ctrl_plugin)
            {
                auto plug = ctrl_plugin->make_applicationPlugin(*gui_ctx);
                registrar.registerApplicationContextPlugin(plug);
            }
        });
    }
    // Load what the plug-ins have to offer.
    for(const iscore::Addon& addon : availablePlugins)
    {
        auto commands_plugin = dynamic_cast<CommandFactory_QtInterface*> (addon.plugin);
        if(commands_plugin)
        {
            registrar.registerCommands(commands_plugin->make_commands());
        }

        auto factories_plugin = dynamic_cast<FactoryInterface_QtInterface*> (addon.plugin);
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

