#include "DeviceExplorerPlugin.hpp"
#include "DeviceExplorerPanelFactory.hpp"
using namespace iscore;

DeviceExplorerPlugin::DeviceExplorerPlugin() :
    QObject {},
iscore::PanelFactoryInterface_QtInterface {}
{
    setObjectName("DeviceExplorerPlugin");
}

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
