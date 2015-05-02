#include <NetworkPlugin.hpp>
#include <NetworkControl.hpp>
#include <settings_impl/NetworkSettings.hpp>
#include "DistributedScenario/Panel/GroupPanelFactory.hpp"

#include "DistributedScenario/Group.hpp"
#define PROCESS_NAME "Network Process"

NetworkPlugin::NetworkPlugin() :
    QObject {},
        iscore::PluginControlInterface_QtInterface {}//,
        //iscore::SettingsDelegateFactoryInterface_QtInterface {}
{
    setObjectName("NetworkPlugin");
}

// Interfaces implementations :
//////////////////////////
/*
iscore::SettingsDelegateFactoryInterface* NetworkPlugin::settings_make()
{
    return new NetworkSettings;
}
*/
iscore::PluginControlInterface* NetworkPlugin::control()
{
    return new NetworkControl;
}

QList<iscore::PanelFactoryInterface*> NetworkPlugin::panels()
{
    return {new GroupPanelFactory};
}

