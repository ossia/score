#include "DeviceExplorerPlugin.hpp"
#include "DeviceExplorerPanelFactory.hpp"
using namespace iscore;
#include "DeviceExplorerControl.hpp"


iscore_plugin_deviceexplorer::iscore_plugin_deviceexplorer() :
    QObject {},
iscore::PanelFactory_QtInterface {}
{
}

QList<PanelFactory*> iscore_plugin_deviceexplorer::panels()
{
    return {new DeviceExplorerPanelFactory};
}



QVector<iscore::FactoryFamily> iscore_plugin_deviceexplorer::factoryFamilies()
{
    return {{"Protocol",
            [] (iscore::FactoryInterface* f)
            { SingletonProtocolList::instance().registerFactory(f); }}};
}

PluginControlInterface *iscore_plugin_deviceexplorer::make_control(Presenter* pres)
{
    return new DeviceExplorerControl{pres};
}
