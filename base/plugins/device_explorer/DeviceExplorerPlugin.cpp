#include "DeviceExplorerPlugin.hpp"
#include "DeviceExplorerPanelFactory.hpp"
using namespace iscore;

DeviceExplorerPlugin::DeviceExplorerPlugin() :
    QObject {},
        iscore::Autoconnect_QtInterface {},
//	iscore::PluginControlInterface_QtInterface{},
iscore::PanelFactoryInterface_QtInterface {}
{
    setObjectName("DeviceExplorerPlugin");
}





QList<Autoconnect> DeviceExplorerPlugin::autoconnect_list() const
{
    return
    {

    };
}


/*
QStringList DeviceExplorerPlugin::control_list() const
{
	return {""};
}

PluginControlInterface* DeviceExplorerPlugin::control_make(QString)
{
	return nullptr;
}*/



QStringList DeviceExplorerPlugin::panel_list() const
{
    return {"DeviceExplorer Panel"};
}

PanelFactoryInterface* DeviceExplorerPlugin::panel_make(QString name)
{
    if(name == "DeviceExplorer Panel")
    {
        return new DeviceExplorerPanelFactory;
    }

    return nullptr;
}
