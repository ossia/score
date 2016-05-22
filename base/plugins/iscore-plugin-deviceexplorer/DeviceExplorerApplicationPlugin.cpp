#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>

#include <algorithm>
#include <vector>

#include "DeviceExplorerApplicationPlugin.hpp"
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <core/document/DocumentModel.hpp>

struct VisitorVariant;

namespace Explorer
{
DeviceExplorerApplicationPlugin::DeviceExplorerApplicationPlugin(
        const iscore::ApplicationContext& app) :
    GUIApplicationContextPlugin {app}
{

}

void DeviceExplorerApplicationPlugin::on_newDocument(iscore::Document* doc)
{
    doc->model().addPluginModel(
                new DeviceDocumentPlugin{doc->context(), &doc->model()});
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
}

