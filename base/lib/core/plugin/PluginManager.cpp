#include <core/plugin/PluginManager.hpp>
#include <core/application/Application.hpp>

#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/DocumentDelegateFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/SettingsDelegateFactoryInterface_QtInterface.hpp>

#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>

#include <QDir>
#include <QSettings>

#include <boost/range/algorithm.hpp>

using namespace iscore;

QStringList pluginsDir()
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

PluginManager::PluginManager(Application* app):
    m_app{app}
{
}

PluginManager::~PluginManager()
{
    clearPlugins();
}

void PluginManager::reloadPlugins()
{
    clearPlugins();
    auto folders = pluginsDir();

    // Load static plug-ins
    for(QObject* plugin : QPluginLoader::staticInstances())
    {
        m_availablePlugins += plugin;
    }

    // Load dynamic plug-ins
    for(const QString& pluginsFolder : folders)
    {
        QDir pluginsDir(pluginsFolder);
        for(const QString& fileName : pluginsDir.entryList(QDir::Files))
        {
            loadPlugin(pluginsDir.absoluteFilePath(fileName));
        }
    }

    // Here, it is important not to collapse all the for-loops
    // because for instance a control from plugin B might require the factory
    // from plugin A to be loaded prior.
    // Load all the factories.
    for(QObject* plugin : m_availablePlugins)
    {
        loadFactories(plugin);
    }

    // Load all the plug-in controls (because all controls need to be initialized for the
    // factories to work, generally).
    for(QObject* plugin : m_availablePlugins)
    {
        loadControls(plugin);
    }

    // Load what the plug-ins have to offer.
    for(QObject* plugin : m_availablePlugins)
    {
        dispatch(plugin);
    }
}

QStringList PluginManager::pluginsOnSystem() const
{
    return m_pluginsOnSystem;
}

void PluginManager::loadPlugin(const QString &fileName)
{
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
            m_availablePlugins += plugin;
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
}

void PluginManager::loadControls(QObject* plugin)
{
    auto ctrl_plugin = qobject_cast<PluginControlInterface_QtInterface*> (plugin);
    if(ctrl_plugin)
    {
        m_controlList.push_back(ctrl_plugin->make_control(m_app->presenter()));
    }
}



void PluginManager::clearPlugins()
{
    for(auto& elt : m_availablePlugins)
        if(elt)
        {
            elt->deleteLater();
        }

    m_availablePlugins.clear();

    for(auto& elt : m_settingsList)
    {
        delete elt;
    }

    for(auto& elt : m_panelList)
    {
        delete elt;
    }

    for(auto& elt : m_documentPanelList)
    {
        delete elt;
    }

    for(auto& vec : m_factoriesInterfaces)
    {
        for(auto& elt : vec)
        {
            delete elt;
        }
    }

    m_customFamilies.clear();
    m_controlList.clear();
    m_panelList.clear();
    m_documentPanelList.clear();
    m_settingsList.clear();
    m_factoriesInterfaces.clear();
}

QStringList PluginManager::pluginsBlacklist()
{
    QSettings s;
    return s.value("PluginSettings/Blacklist", QStringList {}).toStringList();
}


void PluginManager::loadFactories(QObject* plugin)
{
    auto facfam_interface = qobject_cast<FactoryFamily_QtInterface*> (plugin);

    if(facfam_interface)
    {
        m_customFamilies += facfam_interface->factoryFamilies();
    }
}

void PluginManager::dispatch(QObject* plugin)
{
    auto settings_plugin = qobject_cast<SettingsDelegateFactoryInterface_QtInterface*> (plugin);
    auto panel_plugin = qobject_cast<PanelFactory_QtInterface*> (plugin);
    auto docpanel_plugin = qobject_cast<DocumentDelegateFactoryInterface_QtInterface*> (plugin);
    auto factories_plugin = qobject_cast<FactoryInterface_QtInterface*> (plugin);

    if(settings_plugin)
    {// TODO change the name in the correct order.
        m_settingsList.push_back(settings_plugin->settings_make());
    }

    if(panel_plugin)
    {
        m_panelList += panel_plugin->panels();
    }

    if(docpanel_plugin)
    {
        m_documentPanelList += docpanel_plugin->documents();
    }

    if(factories_plugin)
    {
        for(FactoryFamily& factory_family : m_customFamilies)
        {
            auto new_factories = factories_plugin->factories(factory_family.name);

            for(auto new_factory : new_factories)
            {
                factory_family.onInstantiation(new_factory);
            }

            m_factoriesInterfaces.push_back(new_factories);
        }
    }
}
