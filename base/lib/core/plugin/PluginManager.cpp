#include <core/plugin/PluginManager.hpp>


#include <interface/plugins/FactoryInterface_QtInterface.hpp>
#include <interface/plugins/DocumentDelegateFactoryInterface_QtInterface.hpp>
#include <interface/plugins/FactoryFamily_QtInterface.hpp>
#include <interface/plugins/PanelFactoryInterface_QtInterface.hpp>
#include <interface/plugins/PluginControlInterface_QtInterface.hpp>
#include <interface/plugins/SettingsDelegateFactoryInterface_QtInterface.hpp>

#include <interface/customfactory/FactoryInterface.hpp>
#include <interface/panel/PanelFactoryInterface.hpp>
#include <interface/settingsdelegate/SettingsDelegateFactoryInterface.hpp>
#include <interface/documentdelegate/DocumentDelegateFactoryInterface.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QSettings>
#include <QStaticPlugin>

using namespace iscore;

void PluginManager::reloadPlugins()
{
    clearPlugins();
    auto pluginsDir = QDir(qApp->applicationDirPath() + "/plugins");

    auto blacklist = pluginsBlacklist();

    for(QString fileName : pluginsDir.entryList(QDir::Files))
    {
        QPluginLoader loader {pluginsDir.absoluteFilePath(fileName) };

        if(QObject* plugin = loader.instance())
        {
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
            qDebug() << "Error while loading" << fileName << ": " << loader.errorString();
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
    m_commandList.clear();
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
        m_customFamilies += facfam_interface->factoryFamilies_make();
    }
}

void PluginManager::dispatch(QObject* plugin)
{
    auto cmd_plugin = qobject_cast<PluginControlInterface_QtInterface*> (plugin);
    auto settings_plugin = qobject_cast<SettingsDelegateFactoryInterface_QtInterface*> (plugin);
    auto panel_plugin = qobject_cast<PanelFactoryInterface_QtInterface*> (plugin);
    auto docpanel_plugin = qobject_cast<DocumentDelegateFactoryInterface_QtInterface*> (plugin);
    auto factories_plugin = qobject_cast<FactoryInterface_QtInterface*> (plugin);

    if(cmd_plugin)
    {
        for(const auto& cmd : cmd_plugin->control_list())
        {
            m_commandList.push_back(cmd_plugin->control_make(cmd));
        }
    }

    if(settings_plugin)
    {
        m_settingsList.push_back(settings_plugin->settings_make());
    }

    if(panel_plugin)
    {
        for(auto name : panel_plugin->panel_list())
        {
            m_panelList.push_back(panel_plugin->panel_make(name));
        }
    }

    if(docpanel_plugin)
    {
        for(auto name : docpanel_plugin->document_list())
        {
            m_documentPanelList.push_back(docpanel_plugin->document_make(name));
        }
    }

    if(factories_plugin)
    {
        for(FactoryFamily& factory_family : m_customFamilies)
        {
            auto new_factories = factories_plugin->factories_make(factory_family.name);

            for(auto new_factory : new_factories)
            {
                factory_family.onInstantiation(new_factory);
            }

            m_factoriesInterfaces.push_back(new_factories);
        }
    }
}
