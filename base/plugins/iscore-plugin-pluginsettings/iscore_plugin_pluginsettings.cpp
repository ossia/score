#include <iscore_plugin_pluginsettings.hpp>
#include <PluginSettings/PluginSettings.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>

iscore_plugin_pluginsettings::iscore_plugin_pluginsettings()
{
}

iscore_plugin_pluginsettings::~iscore_plugin_pluginsettings()
{

}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_pluginsettings::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::AbstractFactoryKey& key) const
{
    return instantiate_factories<
            iscore::ApplicationContext,
            TL<
            FW<iscore::SettingsDelegateFactory,
                PluginSettings::Factory>
            >>(ctx, key);
}

iscore::Version iscore_plugin_pluginsettings::version() const
{
    return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_pluginsettings::key() const
{
    return "80669523-b942-4aa9-9d3e-15c5bb2b4eb0";
}
