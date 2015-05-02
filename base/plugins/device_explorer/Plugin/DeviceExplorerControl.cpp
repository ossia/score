#include "DeviceExplorerControl.hpp"

#include "Commands/Add/AddAddress.hpp"
#include "Commands/Add/AddDevice.hpp"
#include "Commands/Cut.hpp"
#include "Commands/EditData.hpp"
#include "Commands/Insert.hpp"
#include "Commands/Move.hpp"
#include "Commands/Paste.hpp"
#include "Commands/Remove.hpp"

#include <iscore/command/SerializableCommand.hpp>
DeviceExplorerControl::DeviceExplorerControl() :
    PluginControlInterface {"DeviceExplorerControl", nullptr}
{

}


iscore::SerializableCommand* DeviceExplorerControl::instantiateUndoCommand(
        const QString& name,
        const QByteArray& arr)
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
