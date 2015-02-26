#include <PluginSettingsPlugin.hpp>
#include <settings_impl/PluginSettings.hpp>

PluginSettingsPlugin::PluginSettingsPlugin() :
    QObject {},
iscore::SettingsDelegateFactoryInterface_QtInterface {}
{
    setObjectName ("Plugin Settings");
}

//////////////////////////
iscore::SettingsDelegateFactoryInterface* PluginSettingsPlugin::settings_make()
{
    return new PluginSettings;
}

