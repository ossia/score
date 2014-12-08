#include <core/plugin/PluginManager.hpp>

#include <interface/plugins/ProcessFactoryInterface_QtInterface.hpp>
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

	auto blacklist = pluginsBlacklist(); // TODO prevent the Plugin Settings plugin from being blacklisted
	// TODO the plug-ins should have a "blacklistable" attribute. -> Do a generic iscorePlugin from which they inherit, with this attribute ?
	for(QString fileName : pluginsDir.entryList(QDir::Files))
	{
		QPluginLoader loader{pluginsDir.absoluteFilePath(fileName)};
		if (QObject *plugin = loader.instance())
		{
			if(!blacklist.contains(fileName))
			{
				m_availablePlugins[fileName] = plugin;
				plugin->setParent(this);
				dispatch(plugin);
			}
			else
			{
				plugin->deleteLater();
			}

			m_pluginsOnSystem.push_back(fileName);
		}
	}

	// Load static plug-ins
	for(auto obj : QPluginLoader::staticInstances())
	{
		dispatch(obj);
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

// @todo refactor : make a single loop (use a tuple ? objects? a tempalte function?) or
// make a method return_all in each interface ?
// @todo : make a generic way for plug-ins to register plugin factories. For instance scenario could register a scenario view factory ?
// the PluginFactoryInterface has a dispatch(qobject* ) and does the cast. Must be in two passes : first pass : get the possible interfaces (or use a map ?)
// second pass load the plug-ins.
void PluginManager::dispatch(QObject* plugin)
{
	//qDebug() << plugin->objectName() << "was dispatched";
	auto autoconn_plugin = qobject_cast<Autoconnect_QtInterface*>(plugin);
	auto cmd_plugin = qobject_cast<PluginControlInterface_QtInterface*>(plugin);
	auto settings_plugin = qobject_cast<SettingsDelegateFactoryInterface_QtInterface*>(plugin);
	auto process_plugin = qobject_cast<ProcessFactoryInterface_QtInterface*>(plugin);
	auto panel_plugin = qobject_cast<PanelFactoryInterface_QtInterface*>(plugin);
	auto docpanel_plugin = qobject_cast<DocumentDelegateFactoryInterface_QtInterface*>(plugin);
	auto inspector_plugin = qobject_cast<InspectorWidgetFactoryInterface_QtInterface*>(plugin);

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
		for(const auto& cmd : cmd_plugin->control_list())
		{
			m_commandList.push_back(cmd_plugin->control_make(cmd));
		}
	}

	if(settings_plugin)
	{
		//qDebug() << "The plugin has settings";
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
			m_panelList.push_back(panel_plugin->panel_make(name));
		}
	}

	if(docpanel_plugin)
	{
		//qDebug() << "The plugin adds doc panels";
		for(auto name : docpanel_plugin->document_list())
		{
			m_documentPanelList.push_back(docpanel_plugin->document_make(name));
		}
	}

	if(inspector_plugin)
	{
		for(auto name : inspector_plugin->inspectorFactory_list())
		{
			m_inspectorList.push_back(inspector_plugin->inspectorFactory_make(name));
		}
	}
}
