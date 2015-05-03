#include "DeviceExplorerPlugin.hpp"
#include "DeviceExplorerPanelFactory.hpp"
using namespace iscore;
#include <DeviceExplorer/Protocol/MIDI/MIDIProtocolFactory.hpp>
#include <DeviceExplorer/Protocol/Minuit/MinuitProtocolFactory.hpp>
#include <DeviceExplorer/Protocol/OSC/OSCProtocolFactory.hpp>

#include "DeviceExplorerControl.hpp"


DeviceExplorerPlugin::DeviceExplorerPlugin() :
    QObject {},
iscore::PanelFactoryInterface_QtInterface {}
{
    setObjectName("DeviceExplorerPlugin");
}

QList<PanelFactoryInterface*> DeviceExplorerPlugin::panels()
{
    return {new DeviceExplorerPanelFactory};
}



QVector<iscore::FactoryFamily> DeviceExplorerPlugin::factoryFamilies()
{
    return {{"Protocol",
            [] (iscore::FactoryInterface* f)
            { SingletonProtocolList::instance().registerFactory(f); }}};
}

QVector<iscore::FactoryInterface*> DeviceExplorerPlugin::factories(const QString& factoryName)
{
    if(factoryName == "Protocol")
    {
        return {new MIDIProtocolFactory,
                new MinuitProtocolFactory,
                new OSCProtocolFactory};
    }

    return {};
}


PluginControlInterface *DeviceExplorerPlugin::control()
{
    return new DeviceExplorerControl;
}
