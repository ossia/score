#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>

#include <algorithm>
#include <vector>

#include "DeviceExplorerApplicationPlugin.hpp"
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <core/document/DocumentModel.hpp>

namespace iscore {
class Application;
}  // namespace iscore
struct VisitorVariant;

DeviceExplorerApplicationPlugin::DeviceExplorerApplicationPlugin(iscore::Application& app) :
    GUIApplicationContextPlugin {app, "DeviceExplorerApplicationPlugin", nullptr}
{

}

void DeviceExplorerApplicationPlugin::on_newDocument(iscore::Document* doc)
{
    doc->model().addPluginModel(new DeviceDocumentPlugin{*doc, &doc->model()});
}

iscore::DocumentPluginModel* DeviceExplorerApplicationPlugin::loadDocumentPlugin(
        const QString &name,
        const VisitorVariant &var,
        iscore::Document* doc)
{
    if(name != DeviceDocumentPlugin::staticMetaObject.className())
        return nullptr;

    auto plug = new DeviceDocumentPlugin{*doc, var, &doc->model()};
    return plug;
}

void DeviceExplorerApplicationPlugin::on_documentChanged(
        iscore::Document* olddoc,
        iscore::Document* newdoc)
{
    if(olddoc)
    {
        auto& doc_plugin = olddoc->context().plugin<DeviceDocumentPlugin>();
        doc_plugin.setConnection(false);
    }

    if(newdoc)
    {
        auto& doc_plugin = newdoc->context().plugin<DeviceDocumentPlugin>();
        doc_plugin.setConnection(true);
    }
}

