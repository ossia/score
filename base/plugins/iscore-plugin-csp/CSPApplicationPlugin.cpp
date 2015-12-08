#include "CSPApplicationPlugin.hpp"
#include "CSPDocumentPlugin.hpp"

#include <Process/Process.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>

CSPApplicationPlugin::CSPApplicationPlugin(const iscore::ApplicationContext& pres) :
    iscore::GUIApplicationContextPlugin {pres, "CSPApplicationPlugin", nullptr}
{
}

iscore::DocumentPluginModel* CSPApplicationPlugin::loadDocumentPlugin(
        const QString& name,
        const VisitorVariant& var,
        iscore::Document *parent)
{
    // We don't have anything to load; it's easier to just recreate.
    return nullptr;
}

void
CSPApplicationPlugin::on_newDocument(iscore::Document* document)
{
    if(document)
    {
        document->model().addPluginModel(new CSPDocumentPlugin{*document, &document->model()});
    }
}

void
CSPApplicationPlugin::on_loadedDocument(iscore::Document* document)
{
    if(auto pluginModel = document->context().findPlugin<CSPDocumentPlugin>())
    {
        pluginModel->reload(document->model());
    }
    else
    {
        on_newDocument(document);
    }
}
