#include "DeviceExplorerPlugin.hpp"
#include "DeviceExplorerPanelFactory.hpp"
using namespace iscore;
#include <DeviceExplorer/Protocol/MIDI/MIDIProtocolFactory.hpp>
#include <DeviceExplorer/Protocol/Minuit/MinuitProtocolFactory.hpp>
#include <DeviceExplorer/Protocol/OSC/OSCProtocolFactory.hpp>
#include <DeviceExplorer/Protocol/ProtocolList.hpp>

ProtocolList  SingletonProtocolList::m_instance;
ProtocolList& SingletonProtocolList::instance()
{
    return m_instance;
}

DeviceList  SingletonDeviceList::m_instance;
DeviceList& SingletonDeviceList::instance()
{
    return m_instance;
}




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



QVector<iscore::FactoryFamily> DeviceExplorerPlugin::factoryFamilies_make()
{
    return {{"Protocol",
            [] (iscore::FactoryInterface* f)
            { SingletonProtocolList::instance().registerFactory(f); }}};
}

QVector<iscore::FactoryInterface*> DeviceExplorerPlugin::factories_make(QString factoryName)
{
    if(factoryName == "Protocol")
    {
        return {new MIDIProtocolFactory,
                new MinuitProtocolFactory,
                new OSCProtocolFactory};
    }

    return {};
}
