#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <vector>

namespace iscore
{
class ApplicationContext;
class ApplicationRegistrar;

namespace PluginLoader
{
/**
 * @brief loadPlugins
 *
 * Reloads all the plug-ins.
 * Note: for now this is unsafe after the first loading.
 */
void loadPlugins(iscore::ApplicationRegistrar&,
                 const iscore::ApplicationContext& context);

// QString is set if it's a valid plug-in file,
// QObject* is set if the plug-in could be loaded
std::pair<QString, QObject*> loadPlugin(
        const QString& filename,
        const std::vector<QObject*>& availablePlugins);

QStringList pluginsBlacklist();
}
}
