#include <NetworkPlugin.hpp>
#include <NetworkControl.hpp>
#include <settings_impl/NetworkSettings.hpp>
#include "DistributedScenario/Panel/GroupPanelFactory.hpp"

#include "DistributedScenario/Group.hpp"
#define PROCESS_NAME "Network Process"

iscore_plugin_network::iscore_plugin_network() :
    QObject {},
        iscore::PluginControlInterface_QtInterface {}//,
        //iscore::SettingsDelegateFactoryInterface_QtInterface {}
{
}

// Interfaces implementations :
//////////////////////////
/*
iscore::SettingsDelegateFactoryInterface* NetworkPlugin::settings_make()
{
    return new NetworkSettings;
}
*/
iscore::PluginControlInterface* iscore_plugin_network::make_control(iscore::Presenter* pres)
{
    return new NetworkControl{pres};
}

QList<iscore::PanelFactory*> iscore_plugin_network::panels()
{
    return {new GroupPanelFactory};
}

