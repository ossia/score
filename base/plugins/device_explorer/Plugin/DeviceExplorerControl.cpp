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

#include "DocumentPlugin/DeviceDocumentPlugin.hpp"
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

#include <core/document/DocumentModel.hpp>
void DeviceExplorerControl::on_newDocument(iscore::Document* doc)
{
    doc->model()->addPluginModel(new DeviceDocumentPlugin(doc));

    // connect the device document plugin to the device explorer model in this document
    // (do the connection from the point of view of the device explorer model, it's more sensical)
    //
    // Hence, in the building order, one must come before the other, preferably the document plug-in.
    // hence PluginControl::on_newDocument has to be called first, and then the new PanelModel...
}

void DeviceExplorerControl::on_documentChanged()
{
    // disconnect ...
    // connect(device explorer widget, document->device plugin
}
