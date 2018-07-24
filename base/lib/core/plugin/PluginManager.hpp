#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <core/plugin/PluginDependencyGraph.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/plugins/PluginInstances.hpp>
#include <score_lib_base_export.h>
#include <vector>
namespace score
{
struct ApplicationContext;
class GUIApplicationRegistrar;
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

SCORE_LIB_BASE_EXPORT void loadPluginsInAllFolders(
    std::vector<score::Addon>& availablePlugins, QStringList additional = {});

SCORE_LIB_BASE_EXPORT void
loadAddonsInAllFolders(std::vector<score::Addon>& availablePlugins);

std::pair<score::Plugin_QtInterface*, PluginLoadingError> loadPlugin(
    const QString& fileName,
    const std::vector<score::Addon>& availablePlugins);

ossia::optional<score::Addon> makeAddon(
    const QString& addon_path,
    const QJsonObject& json_addon,
    const std::vector<score::Addon>& availablePlugins);

template <typename Registrar_T>
void registerPluginsImpl(
    const std::vector<score::Addon>& availablePlugins,
    Registrar_T& registrar,
    const score::GUIApplicationContext& context)
{
  // Load what the plug-ins have to offer.
  for (const score::Addon& addon : availablePlugins)
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
        const score::ApplicationContext& base_ctx = context;
        // Register core factories
        for (auto&& new_factory :
             factories_plugin->factories(base_ctx, factory_family.first))
        {
          factory_family.second->insert(std::move(new_factory));
        }

        // Register GUI factories
        for (auto&& new_factory :
             factories_plugin->guiFactories(context, factory_family.first))
        {
          factory_family.second->insert(std::move(new_factory));
        }
      }
    }
  }
}

template <typename Registrar_T>
void registerPlugins(
    const std::vector<score::Addon>& availablePlugins,
    Registrar_T& registrar,
    const score::GUIApplicationContext& context)
{
  for (const score::Addon& addon : availablePlugins)
  {
    auto ctrl_plugin
        = dynamic_cast<ApplicationPlugin_QtInterface*>(addon.plugin);
    if (ctrl_plugin)
    {
      if (auto plug = ctrl_plugin->make_applicationPlugin(context))
        registrar.registerApplicationPlugin(plug);
      if (auto plug = ctrl_plugin->make_guiApplicationPlugin(context))
        registrar.registerGUIApplicationPlugin(plug);
    }
  }
  registerPluginsImpl(availablePlugins, registrar, context);
}

/**
 * @brief loadPlugins
 * @tparam Registrar_T see score::ApplicationRegistrar
 * @tparam Context_T see score::ApplicationContext
 * Reloads all the plug-ins.
 * Note: for now this is unsafe after the first loading.
 */
template <typename Registrar_T, typename Context_T>
void loadPlugins(Registrar_T& registrar, const Context_T& context)
{
  // Here, the plug-ins that are effectively loaded.
  std::vector<score::Addon> availablePlugins;

  // Load static plug-ins
  for (auto score_plug : score::staticPlugins())
  {
    score::Addon addon;
    addon.plugin = score_plug;
    addon.key = score_plug->key();
    addon.corePlugin = true;
    availablePlugins.push_back(std::move(addon));
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
  for (const score::Addon& addon : availablePlugins)
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
  if (!add.empty())
  {
    registerPlugins(add, registrar, context);
  }
}

QStringList pluginsBlacklist();
}
}
