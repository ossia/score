#include "DeviceExplorerControl.hpp"

#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

DeviceExplorerControl::DeviceExplorerControl(iscore::Application& app) :
    PluginControlInterface {app, "DeviceExplorerControl", nullptr}
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

void DeviceExplorerControl::on_documentChanged(
        iscore::Document* olddoc,
        iscore::Document* newdoc)
{
    if(olddoc)
    {
        auto doc_plugin = olddoc->model().pluginModel<DeviceDocumentPlugin>();
        doc_plugin->setConnection(false);
    }

    if(newdoc)
    {
        auto doc_plugin = newdoc->model().pluginModel<DeviceDocumentPlugin>();
        doc_plugin->setConnection(true);
    }
}

