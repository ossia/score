#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <vector>
#include <QPluginLoader>

#include <core/plugin/PluginDependencyGraph.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

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

enum class PluginLoadingError
{
  NoError,
  Blacklisted,
  NotAPlugin,
  AlreadyLoaded,
  UnknownError
};


QStringList addonsDir();
QStringList pluginsDir();

ISCORE_LIB_BASE_EXPORT void loadPluginsInAllFolders(
    std::vector<iscore::Addon>& availablePlugins);

ISCORE_LIB_BASE_EXPORT void loadAddonsInAllFolders(
    std::vector<iscore::Addon>& availablePlugins);

std::pair<iscore::Plugin_QtInterface*, PluginLoadingError>
loadPlugin(
    const QString& fileName,
    const std::vector<iscore::Addon>& availablePlugins);

iscore::optional<iscore::Addon> makeAddon(
    const QString& addon_path,
    const QJsonObject& json_addon,
    const std::vector<iscore::Addon>& availablePlugins);


template<typename Registrar_T, typename Context_T>
void registerPlugins(
    const std::vector<iscore::Addon>& availablePlugins,
    Registrar_T& registrar,
    const Context_T& context)
{
  // Load what the plug-ins have to offer.
  for (const iscore::Addon& addon : availablePlugins)
  {
    auto commands_plugin
        = dynamic_cast<CommandFactory_QtInterface*>(addon.plugin);
    if (commands_plugin)
    {
      registrar.registerCommands(commands_plugin->make_commands());
    }

    auto factories_plugin
        = dynamic_cast<FactoryInterface_QtInterface*>(addon.plugin);
    if (factories_plugin)
    {
      for (auto& factory_family : registrar.components().factories)
      {
        for (auto&& new_factory :
             factories_plugin->factories(context, factory_family.first))
        {
          factory_family.second->insert(std::move(new_factory));
        }
      }
    }
  }
}


template<typename Registrar_T>
void registerPlugins(
    const std::vector<iscore::Addon>& availablePlugins,
    Registrar_T& registrar,
    const iscore::GUIApplicationContext& context)
{
  for(const iscore::Addon& addon : availablePlugins)
  {
    auto ctrl_plugin
        = dynamic_cast<GUIApplicationContextPlugin_QtInterface*>(addon.plugin);
    if (ctrl_plugin)
    {
      if (auto plug = ctrl_plugin->make_applicationPlugin(context))
        registrar.registerApplicationContextPlugin(plug);
    }
  }
  registerPlugins(availablePlugins, registrar, (const iscore::ApplicationContext&) context);
}

/**
 * @brief loadPlugins
 * @tparam Registrar_T see iscore::ApplicationRegistrar
 * @tparam Context_T see iscore::ApplicationContext
 * Reloads all the plug-ins.
 * Note: for now this is unsafe after the first loading.
 */
template<typename Registrar_T, typename Context_T>
void loadPlugins(
    Registrar_T& registrar, const Context_T& context)
{
  // Here, the plug-ins that are effectively loaded.
  std::vector<iscore::Addon> availablePlugins;

  // Load static plug-ins
  for (QObject* plugin : QPluginLoader::staticInstances())
  {
    if (auto iscore_plug = dynamic_cast<iscore::Plugin_QtInterface*>(plugin))
    {
      iscore::Addon addon;
      addon.corePlugin = true;
      addon.plugin = iscore_plug;
      availablePlugins.push_back(std::move(addon));
    }
  }

  loadPluginsInAllFolders(availablePlugins);
  loadAddonsInAllFolders(availablePlugins);

  // First bring in the plugin objects
  registrar.registerAddons(availablePlugins);

  // Here, it is important not to collapse all the for-loops
  // because for instance a ApplicationPlugin from plugin B might require the
  // factory
  // from plugin A to be loaded prior.
  // Load all the factories.
  for (const iscore::Addon& addon : availablePlugins)
  {
    auto facfam_interface
        = dynamic_cast<FactoryList_QtInterface*>(addon.plugin);

    if (facfam_interface)
    {
      for (auto&& elt : facfam_interface->factoryFamilies())
      {
        registrar.registerFactory(std::move(elt));
      }
    }
  }

  // Load all the application context plugins.
  // We have to order them according to their dependencies
  PluginDependencyGraph graph{availablePlugins};
  const auto& add = graph.sortedAddons();
  if(!add.empty())
  {
    registerPlugins(add, registrar, context);
  }
}

QStringList pluginsBlacklist();
}
}
