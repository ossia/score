#include <core/plugin/PluginManager.h>

#include <plugin_interface/ProcessFactoryPluginInterface.h>
#include <QCoreApplication>
#include <QDir>

using namespace iscore;

void PluginManager::loadPlugins()
{
    auto pluginsDir = QDir(qApp->applicationDirPath() + "/plugins");

    for(QString fileName : pluginsDir.entryList(QDir::Files))
    {
        QPluginLoader loader(pluginsDir.absoluteFilePath(fileName));
        QObject *plugin = loader.instance();
        if (plugin)
        {
            auto casted_plugin = qobject_cast<ProcessFactoryPluginInterface*>(plugin);
            if(casted_plugin)
            {
                auto custom_process = casted_plugin->process_make(casted_plugin->process_list().first());
                auto model = custom_process->makeModel();
            }
        }
    }
}
