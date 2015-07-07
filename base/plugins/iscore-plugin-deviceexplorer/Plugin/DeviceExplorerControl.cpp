#include "DeviceExplorerControl.hpp"

#include "Commands/Add/AddAddress.hpp"
#include "Commands/Add/AddDevice.hpp"
#include "Commands/Cut.hpp"
#include "Commands/EditData.hpp"
#include "Commands/Insert.hpp"
#include "Commands/Move.hpp"
#include "Commands/Paste.hpp"
#include "Commands/Remove.hpp"
#include "Commands/UpdateNamespace.hpp"

#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include "DocumentPlugin/DeviceDocumentPlugin.hpp"
DeviceExplorerControl::DeviceExplorerControl(iscore::Presenter* pres) :
    PluginControlInterface {pres, "DeviceExplorerControl", nullptr}
{
    setupCommands();
}


struct DeviceExplorerCommandFactory
{
        static CommandGeneratorMap map;
};

CommandGeneratorMap DeviceExplorerCommandFactory::map;

void DeviceExplorerControl::setupCommands()
{
    using namespace DeviceExplorer::Command;
    boost::mpl::for_each<
            boost::mpl::list<
            AddAddress,
            AddDevice,
            Cut,
            EditData,
            Insert,
            Move,
            Paste,
            Remove,
            ReplaceDevice
            >,
            boost::type<boost::mpl::_>
    >(CommandGeneratorMapInserter<DeviceExplorerCommandFactory>());
}

iscore::SerializableCommand* DeviceExplorerControl::instantiateUndoCommand(
        const QString& name,
        const QByteArray& data)
{
    return PluginControlInterface::instantiateUndoCommand<DeviceExplorerCommandFactory>(name, data);
}


#include <core/document/DocumentModel.hpp>
void DeviceExplorerControl::on_newDocument(iscore::Document* doc)
{
    ISCORE_TODO;
    doc->model()->addPluginModel(new DeviceDocumentPlugin(doc));

    // connect the device document plugin to the device explorer model in this document
    // (do the connection from the point of view of the device explorer model, it's more sensical)
    //
    // Hence, in the building order, one must come before the other, preferably the document plug-in.
    // hence PluginControl::on_newDocument has to be called first, and then the new PanelModel...

    // But the device explorer model must save its state! So maybe should we reload from this one instead ?
    // Also, add functions to enable / disable the connections to the remote devices (or maybe put a LED indicator ?)
    // and add exploring.
}

iscore::DocumentDelegatePluginModel* DeviceExplorerControl::loadDocumentPlugin(
        const QString &name,
        const VisitorVariant &var,
        iscore::DocumentModel *model)
{
    return new DeviceDocumentPlugin(var, model);
}

void DeviceExplorerControl::on_documentChanged()
{
    // disconnect ...
    // connect(device explorer widget, document->device plugin
}

