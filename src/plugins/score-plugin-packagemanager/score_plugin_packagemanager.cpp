#include "score_plugin_packagemanager.hpp"

#include <PackageManager/Factory.hpp>
#include <score_plugin_library.hpp>
#include <score/plugins/FactorySetup.hpp>
score_plugin_packagemanager::score_plugin_packagemanager()
{
}

score_plugin_packagemanager::~score_plugin_packagemanager() {}

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_packagemanager::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<score::SettingsDelegateFactory
      , PM::Factory>
      >(ctx, key);
}

std::vector<score::PluginKey> score_plugin_packagemanager::required() const
{
  return {score_plugin_library::static_key()};
}

#include <score/plugins/PluginInstances.hpp>

SCORE_EXPORT_PLUGIN(score_plugin_packagemanager)
