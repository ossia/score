#include <PluginSettingsPlugin.hpp>
#include <PluginSettings/PluginSettings.hpp>

#include <iscore/plugins/qt_interfaces/SettingsDelegateFactoryInterface_QtInterface.hpp>

iscore_plugin_pluginsettings::iscore_plugin_pluginsettings()
{
}

iscore_plugin_pluginsettings::~iscore_plugin_pluginsettings()
{

}

//////////////////////////

/*
iscore::SettingsDelegateFactoryInterface* iscore_plugin_pluginsettings::settings_make()
{
    return new PluginSettings::PluginSettingsFactory;
}
*/

iscore::Version iscore_plugin_pluginsettings::version() const
{
    return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_pluginsettings::key() const
{
    return "80669523-b942-4aa9-9d3e-15c5bb2b4eb0";
}
