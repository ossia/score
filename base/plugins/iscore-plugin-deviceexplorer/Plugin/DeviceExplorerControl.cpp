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
    doc->model()->addPluginModel(new DeviceDocumentPlugin(doc));
}

iscore::DocumentDelegatePluginModel* DeviceExplorerControl::loadDocumentPlugin(
        const QString &name,
        const VisitorVariant &var,
        iscore::DocumentModel *model)
{
    if(name != DeviceDocumentPlugin::staticMetaObject.className())
        return nullptr;

    auto plug = new DeviceDocumentPlugin(var, model);
    return plug;
}

void DeviceExplorerControl::on_documentChanged()
{
    // disconnect ...
    // connect(device explorer widget, document->device plugin
}

