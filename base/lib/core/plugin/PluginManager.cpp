#include <core/plugin/PluginManager.hpp>


#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/DocumentDelegateFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/SettingsDelegateFactoryInterface_QtInterface.hpp>

#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/panel/PanelFactory.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactoryInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

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

void PluginManager::reloadPlugins()
{
    clearPlugins();
    auto folders = pluginsDir();
    auto blacklist = pluginsBlacklist();

    for(const QString& pluginsFolder : folders)
    {
        QDir pluginsDir(pluginsFolder);
        for(const QString& fileName : pluginsDir.entryList(QDir::Files))
        {
            QPluginLoader loader {pluginsDir.absoluteFilePath(fileName) };

            if(QObject* plugin = loader.instance())
            {
                // TODO check if the plug-in isn't already loaded.
                if(!blacklist.contains(fileName))
                {
                    m_availablePlugins[plugin->objectName()] = plugin;
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
    }
    // Load static plug-ins
    for(QObject* plugin : QPluginLoader::staticInstances())
    {
        m_availablePlugins[plugin->objectName()] = plugin;
    }

    // Load all the factories.
    for(QObject* plugin : m_availablePlugins)
    {
        loadFactories(plugin);
    }

    // Load what the plug-ins have to offer.
    for(QObject* plugin : m_availablePlugins)
    {
        dispatch(plugin);
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
    auto cmd_plugin = qobject_cast<PluginControlInterface_QtInterface*> (plugin);
    auto settings_plugin = qobject_cast<SettingsDelegateFactoryInterface_QtInterface*> (plugin);
    auto panel_plugin = qobject_cast<PanelFactory_QtInterface*> (plugin);
    auto docpanel_plugin = qobject_cast<DocumentDelegateFactoryInterface_QtInterface*> (plugin);
    auto factories_plugin = qobject_cast<FactoryInterface_QtInterface*> (plugin);

    if(cmd_plugin)
    {
        m_controlList.push_back(cmd_plugin->control());
    }

    if(settings_plugin)
    {
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
