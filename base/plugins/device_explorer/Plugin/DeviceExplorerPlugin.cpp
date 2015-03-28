#include "DeviceExplorerPlugin.hpp"
#include "DeviceExplorerPanelFactory.hpp"
using namespace iscore;
#include <DeviceExplorer/Protocol/ProtocolInterface.hpp>
#include <DeviceExplorer/Protocol/MIDI/MIDIProtocolFactory.hpp>
#include <DeviceExplorer/Protocol/Minuit/MinuitProtocolFactory.hpp>
#include <DeviceExplorer/Protocol/OSC/OSCProtocolFactory.hpp>

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
    return {};
    /*
    return {{"Protocol",
            std::bind(&ProtocolList::registerProtocol,
                      m_control->protocolList(),
                      std::placeholders::_1)}
    };*/
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
