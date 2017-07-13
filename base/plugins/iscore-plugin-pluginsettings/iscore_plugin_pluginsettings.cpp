// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <PluginSettings/PluginSettings.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>
#include <iscore_plugin_pluginsettings.hpp>

iscore_plugin_pluginsettings::iscore_plugin_pluginsettings()
{
}

iscore_plugin_pluginsettings::~iscore_plugin_pluginsettings()
{
}

std::vector<std::unique_ptr<iscore::InterfaceBase>>
iscore_plugin_pluginsettings::factories(
    const iscore::ApplicationContext& ctx,
    const iscore::InterfaceKey& key) const
{
  return instantiate_factories<iscore::ApplicationContext, FW<iscore::SettingsDelegateFactory, PluginSettings::Factory>>(
      ctx, key);
}
