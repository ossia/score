#include <core/plugin/PluginManager.hpp>

#include <interface/plugins/ProcessFactoryPluginInterface.hpp>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
using namespace iscore;


/*
 * Example of usage of a plugin:
 *

auto casted_plugin = qobject_cast<ProcessFactoryPluginInterface*>(plugin);
if(casted_plugin)
{
	auto custom_process = casted_plugin->process_make(casted_plugin->process_list().first());
	auto model = custom_process->makeModel();
}

 *
 *
 */


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
			emit newPlugin(plugin);
		}
	}
}

void PluginManager::clearPlugins()
{
	for(auto& elt : m_availablePlugins)
		delete elt;

	m_availablePlugins.clear();
}
