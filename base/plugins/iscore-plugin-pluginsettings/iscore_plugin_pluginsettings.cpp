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
  return instantiate_factories<iscore::ApplicationContext, TL<FW<iscore::SettingsDelegateFactory, PluginSettings::Factory>>>(
      ctx, key);
}

iscore::Version iscore_plugin_pluginsettings::version() const
{
  return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_pluginsettings::key() const
{
  return_uuid("f3407ffc-bb6a-494c-9a6e-d4f40028769e");
}
