#include <NetworkPlugin.hpp>
#include <NetworkCommand.hpp>
#include <settings_impl/NetworkSettings.hpp>

#define PROCESS_NAME "Network Process"
#define CMD_NAME "Networkigate"
#define MAIN_PANEL_NAME "NetworkCentralPanel"
#define SECONDARY_PANEL_NAME "NetworkSmallPanel"

NetworkPlugin::NetworkPlugin() :
    QObject {},
        iscore::PluginControlInterface_QtInterface {},
iscore::SettingsDelegateFactoryInterface_QtInterface {}
{
    setObjectName("NetworkPlugin");
}

// Interfaces implementations :
//////////////////////////
iscore::SettingsDelegateFactoryInterface* NetworkPlugin::settings_make()
{
    return new NetworkSettings;
}

//////////////////////////
QStringList NetworkPlugin::control_list() const
{
    return {CMD_NAME};
}

iscore::PluginControlInterface* NetworkPlugin::control_make(QString name)
{
    if(name == QString(CMD_NAME))
    {
        return new NetworkControl;
    }

    return nullptr;
}

