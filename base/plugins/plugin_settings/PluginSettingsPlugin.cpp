#include <PluginSettingsPlugin.hpp>
#include <settings_impl/PluginSettings.hpp>

iscore_plugin_pluginsettings::iscore_plugin_pluginsettings() :
    QObject {},
iscore::SettingsDelegateFactoryInterface_QtInterface {}
{
}

//////////////////////////
iscore::SettingsDelegateFactoryInterface* iscore_plugin_pluginsettings::settings_make()
{
    return new PluginSettings;
}

