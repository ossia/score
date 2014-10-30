#include <core/plugin/PluginManager.hpp>

#include <interface/plugins/ProcessFactoryPluginInterface.hpp>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
using namespace iscore;

void PluginManager::reloadPlugins()
{
	clearPlugins();
	auto pluginsDir = QDir(qApp->applicationDirPath() + "/plugins");

	for(QString fileName : pluginsDir.entryList(QDir::Files))
	{
		QPluginLoader loader{pluginsDir.absoluteFilePath(fileName)};
		if (QObject *plugin = loader.instance())
		{
			m_availablePlugins[fileName] = plugin;
			plugin->setParent(this);
			dispatch(plugin);
		}
	}
}

void PluginManager::clearPlugins()
{
	for(auto& elt : m_availablePlugins)
		if(elt) elt->deleteLater();

	m_availablePlugins.clear();
}


void PluginManager::dispatch(QObject* plugin)
{
	//qDebug() << plugin->objectName() << "was dispatched";
	auto autoconn_plugin = qobject_cast<AutoconnectFactoryPluginInterface*>(plugin);
	auto cmd_plugin = qobject_cast<CustomCommandFactoryPluginInterface*>(plugin);
	auto settings_plugin = qobject_cast<SettingsFactoryPluginInterface*>(plugin);
	auto process_plugin = qobject_cast<ProcessFactoryPluginInterface*>(plugin);
	auto panel_plugin = qobject_cast<PanelFactoryPluginInterface*>(plugin);

	if(autoconn_plugin)
	{
		//qDebug() << "The plugin has auto-connections";
		// I auto-connect stuff
		for(const auto& connection : autoconn_plugin->autoconnect_list())
		{
			m_autoconnections.push_back(connection);
		}
	}

	if(cmd_plugin)
	{
		//qDebug() << "The plugin adds menu options";
		for(const auto& cmd : cmd_plugin->customCommand_list())
		{
			//m_presenter->addCustomCommand(menu_plugin->customCommand_make(cmd));
			m_commandList.push_back(cmd_plugin->customCommand_make(cmd));
		}
	}

	if(settings_plugin)
	{
		//qDebug() << "The plugin has settings";
		//m_settings->addSettingsPlugin(settings_plugin->settings_make());
		m_settingsList.push_back(settings_plugin->settings_make());
	}

	if(process_plugin)
	{
		// Ajouter à la liste des process disponibles
		//qDebug() << "The plugin has custom processes";

		for(auto procname : process_plugin->process_list())
			m_processList.addProcess(process_plugin->process_make(procname));
	}

	if(panel_plugin)
	{
		//qDebug() << "The plugin adds panels";
		for(auto name : panel_plugin->panel_list())
		{
			//qDebug() << name;
			//m_presenter->addPanel(panel_plugin->panel_make(name));
			m_panelList.push_back(panel_plugin->panel_make(name));
		}
	}
}
