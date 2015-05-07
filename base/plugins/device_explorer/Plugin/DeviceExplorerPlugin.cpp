#include "DeviceExplorerPlugin.hpp"
#include "DeviceExplorerPanelFactory.hpp"
using namespace iscore;
#include "DeviceExplorerControl.hpp"


DeviceExplorerPlugin::DeviceExplorerPlugin() :
    QObject {},
iscore::PanelFactory_QtInterface {}
{
}

QList<PanelFactory*> DeviceExplorerPlugin::panels()
{
    return {new DeviceExplorerPanelFactory};
}



QVector<iscore::FactoryFamily> DeviceExplorerPlugin::factoryFamilies()
{
    return {{"Protocol",
            [] (iscore::FactoryInterface* f)
            { SingletonProtocolList::instance().registerFactory(f); }}};
}

PluginControlInterface *DeviceExplorerPlugin::control()
{
    return new DeviceExplorerControl;
}
