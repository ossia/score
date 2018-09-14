// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <PluginSettings/PluginSettings.hpp>

#include <score/plugins/customfactory/FactorySetup.hpp>

#include <score_plugin_pluginsettings.hpp>

score_plugin_pluginsettings::score_plugin_pluginsettings() = default;
score_plugin_pluginsettings::~score_plugin_pluginsettings() = default;

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_pluginsettings::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<score::SettingsDelegateFactory, PluginSettings::Factory>>(ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_pluginsettings)
