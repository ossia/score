#include <core/plugin/PluginManager.hpp>

#include <interface/plugins/ProcessFactoryInterface_QtInterface.hpp>
#include <interface/plugins/CustomFactoryInterface_QtInterface.hpp>
#include <interface/plugins/FactoryFamily_QtInterface.hpp>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QSettings>
#include <QStaticPlugin>

using namespace iscore;

void PluginManager::reloadPlugins()
{
	// @todo to do this while running, maybe save the document transparently in memory, and reload it aftewards ?
	clearPlugins();
	auto pluginsDir = QDir(qApp->applicationDirPath() + "/plugins");

	auto blacklist = pluginsBlacklist(); // TODO prevent the Plugin Settings plugin from being blacklisted
	// TODO the plug-ins should have a "blacklistable" attribute. -> Do a generic iscorePlugin from which they inherit, with this attribute ?
	for(QString fileName : pluginsDir.entryList(QDir::Files))
	{
		QPluginLoader loader{pluginsDir.absoluteFilePath(fileName)};
		if (QObject *plugin = loader.instance())
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
		if(elt) elt->deleteLater();

	m_availablePlugins.clear();
}

QStringList PluginManager::pluginsBlacklist()
{
	QSettings s;
	// TODO stock the key in some generic place.
	return s.value("PluginSettings/Blacklist", QStringList{}).toStringList();
}


void PluginManager::loadFactories(QObject* plugin)
{
	auto facfam_interface = qobject_cast<FactoryFamily_QtInterface*>(plugin);
	if(facfam_interface)
	{
		m_customFactories += facfam_interface->factoryFamilies_make();
	}
}

// @todo refactor : make a single loop (use a tuple ? objects? a tempalte function?) or
// make a method return_all in each interface ?
// @todo : make a generic way for plug-ins to register plugin factories. For instance scenario could register a scenario view factory ?
// the PluginFactoryInterface has a dispatch(qobject* ) and does the cast.
// Must be in two passes :
//   first pass : get the possible interfaces (or use a map ?)
//   second pass: load the plug-ins.
void PluginManager::dispatch(QObject* plugin)
{
	auto autoconn_plugin = qobject_cast<Autoconnect_QtInterface*>(plugin);
	auto cmd_plugin = qobject_cast<PluginControlInterface_QtInterface*>(plugin);
	auto settings_plugin = qobject_cast<SettingsDelegateFactoryInterface_QtInterface*>(plugin);
	auto panel_plugin = qobject_cast<PanelFactoryInterface_QtInterface*>(plugin);
	auto docpanel_plugin = qobject_cast<DocumentDelegateFactoryInterface_QtInterface*>(plugin);
	auto factories_plugin = qobject_cast<FactoryInterface_QtInterface*>(plugin);

	if(autoconn_plugin)
	{
		for(const auto& connection : autoconn_plugin->autoconnect_list())
		{
			m_autoconnections.push_back(connection);
		}
	}

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
		for(FactoryFamily& factory_family : m_customFactories)
		{
			auto new_factories = factories_plugin->factories_make(factory_family.name);
			for(auto new_factory : new_factories)
				factory_family.onInstantiation(new_factory);
		}
	}
}
