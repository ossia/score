#include "DeviceExplorerPlugin.hpp"
#include "DeviceExplorerPanelFactory.hpp"
using namespace iscore;
#include <DeviceExplorer/Protocol/MIDI/MIDIProtocolFactory.hpp>
#include <DeviceExplorer/Protocol/Minuit/MinuitProtocolFactory.hpp>
#include <DeviceExplorer/Protocol/OSC/OSCProtocolFactory.hpp>


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

#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include "Commands/Add/AddAddress.hpp"
#include "Commands/Add/AddDevice.hpp"
#include "Commands/Cut.hpp"
#include "Commands/EditData.hpp"
#include "Commands/Insert.hpp"
#include "Commands/Move.hpp"
#include "Commands/Paste.hpp"
#include "Commands/Remove.hpp"
class DeviceExplorerControl : public iscore::PluginControlInterface
{
    public:
        DeviceExplorerControl() :
            PluginControlInterface {"DeviceExplorerControl", nullptr}
        {

        }


        virtual SerializableCommand *instantiateUndoCommand(const QString & name,
                                                            const QByteArray & arr)
        {
            using namespace DeviceExplorer::Command;
            iscore::SerializableCommand* cmd{};
            if(name == "AddAddress")
            {
                cmd = new AddAddress;
            }
            else if(name == "AddDevice")
            {
                cmd = new AddDevice;
            }
            else if(name == "Cut")
            {
                cmd = new Cut;
            }
            else if(name == "EditData")
            {
                cmd = new EditData;
            }
            else if(name == "Insert")
            {
                cmd = new Insert;
            }
            else if(name == "Move")
            {
                cmd = new Move;
            }
            else if(name == "Paste")
            {
                cmd = new Paste;
            }
            else if(name == "Remove")
            {
                cmd = new Remove;
            }
            else
            {
                qDebug() << Q_FUNC_INFO << "Warning : command" << name << "received, but it could not be read.";
                return nullptr;
            }

            cmd->deserialize(arr);
            return cmd;
        }
};

PluginControlInterface *DeviceExplorerPlugin::control_make()
{
    return new DeviceExplorerControl;

}
