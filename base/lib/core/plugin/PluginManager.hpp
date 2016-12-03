#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <vector>

#include <iscore_lib_base_export.h>
namespace iscore
{
struct ApplicationContext;
class ApplicationRegistrar;
class Plugin_QtInterface;
struct Addon;

/**
 * \namespace PluginLoader
 * \brief Classes and functions used at the plug-in loading step.
 */
namespace PluginLoader
{
/**
 * @brief loadPlugins
 *
 * Reloads all the plug-ins.
 * Note: for now this is unsafe after the first loading.
 */
ISCORE_LIB_BASE_EXPORT void loadPlugins(
    iscore::ApplicationRegistrar&, const iscore::ApplicationContext& context);

QStringList pluginsBlacklist();
}
}
