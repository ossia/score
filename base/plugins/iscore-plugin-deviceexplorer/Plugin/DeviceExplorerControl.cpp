#include "DeviceExplorerControl.hpp"

#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include "DocumentPlugin/DeviceDocumentPlugin.hpp"
#include <core/document/DocumentModel.hpp>
DeviceExplorerControl::DeviceExplorerControl(iscore::Presenter* pres) :
    PluginControlInterface {pres, "DeviceExplorerControl", nullptr}
{

}

void DeviceExplorerControl::on_newDocument(iscore::Document* doc)
{
    doc->model().addPluginModel(new DeviceDocumentPlugin(doc));
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

