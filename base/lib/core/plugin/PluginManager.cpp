#include <core/plugin/PluginManager.hpp>
#include <core/application/Application.hpp>

#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/DocumentDelegateFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/SettingsDelegateFactoryInterface_QtInterface.hpp>

#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>

#include <core/application/ApplicationRegistrar.hpp>
#include <core/application/ApplicationComponents.hpp>
#include <QDir>
#include <QSettings>
#include <QPluginLoader>
#include "PluginDependencyGraph.hpp"
#include <boost/range/algorithm.hpp>

using namespace iscore;

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
#endif
}

PluginLoader::PluginLoader(Application* app):
    m_app{app}
{
}

PluginLoader::~PluginLoader()
{
    clearPlugins();
}


QStringList PluginLoader::pluginsOnSystem() const
{
    return m_pluginsOnSystem;
}

void PluginLoader::loadPlugin(const QString &fileName)
{
#if !defined(ISCORE_STATIC_QT)
    auto blacklist = pluginsBlacklist();
    QPluginLoader loader {fileName};

    if(QObject* plugin = loader.instance())
    {
        // Check if the plugin is not already loaded
        if(boost::range::find_if(m_availablePlugins,
           [&] (QObject* obj)
           { return obj->metaObject()->className() == plugin->metaObject()->className(); })
                != m_availablePlugins.end())
        {
            qDebug() << "Warning: plugin"
                     << plugin->metaObject()->className()
                     << "was already loaded. Not reloading.";

            return;
        }

        // Check if it is blacklisted
        if(!blacklist.contains(fileName))
        {
            m_availablePlugins.push_back(plugin);
            plugin->setParent(this);
        }
        else
        {
            plugin->deleteLater();
        }

        m_pluginsOnSystem.push_back(fileName);
    }
    else
    {
        QString s = loader.errorString();
        if(!s.contains("Plugin verification data mismatch") && !s.contains("is not a Qt plugin"))
            qDebug() << "Error while loading" << fileName << ": " << loader.errorString();
    }
#endif
}

void PluginLoader::reloadPlugins(iscore::ApplicationRegistrar& registrar)
{
    clearPlugins();
    auto folders = pluginsDir();

    // Load static plug-ins
    for(QObject* plugin : QPluginLoader::staticInstances())
    {
        m_availablePlugins.push_back(plugin);
    }

#if !defined(ISCORE_STATIC_QT)
    // Load dynamic plug-ins
    for(const QString& pluginsFolder : folders)
    {
        QDir pluginsDir(pluginsFolder);
        for(const QString& fileName : pluginsDir.entryList(QDir::Files))
        {
            loadPlugin(pluginsDir.absoluteFilePath(fileName));
        }
    }
#endif

    // Here, it is important not to collapse all the for-loops
    // because for instance a control from plugin B might require the factory
    // from plugin A to be loaded prior.
    // Load all the factories.
    for(QObject* plugin : m_availablePlugins)
    {
        auto facfam_interface = qobject_cast<FactoryList_QtInterface*> (plugin);

        if(facfam_interface)
        {
            auto other = facfam_interface->factoryFamilies();
            for(auto elt : other)
            {
                registrar.registerFactory(elt);
            }
        }
    }

    // Load all the plug-in controls (because all controls need to be initialized for the
    // factories to work, generally).
    // We have to order them according to their dependencies
    PluginDependencyGraph graph;
    for(QObject* plugin : m_availablePlugins)
    {
        graph.addNode(plugin);
    }

    graph.visit([&] (QObject* plugin) {
        auto ctrl_plugin = qobject_cast<GUIApplicationContextPlugin_QtInterface*> (plugin);
        if(ctrl_plugin)
        {
            auto plug = ctrl_plugin->make_applicationPlugin(*m_app);
            registrar.registerApplicationContextPlugin(plug);
        }
    });

    // Load what the plug-ins have to offer.
    for(QObject* plugin : m_availablePlugins)
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

        ApplicationContext c{*m_app};
        auto factories_plugin = qobject_cast<FactoryInterface_QtInterface*> (plugin);
        if(factories_plugin)
        {
            for(auto factory_family : registrar.components().factories)
            {
                auto new_factories = factories_plugin->factories(c, factory_family.first);
                for(auto new_factory : new_factories)
                {
                    factory_family.second->insert(new_factory);
                }
            }
        }
    }
}


void PluginLoader::clearPlugins()
{
    for(auto& elt : m_availablePlugins)
        if(elt)
        {
            elt->deleteLater();
        }

    m_availablePlugins.clear();
}

QStringList PluginLoader::pluginsBlacklist()
{
    QSettings s;
    return s.value("PluginSettings/Blacklist", QStringList {}).toStringList();
}

