#include "CSPControl.hpp"
#include "CSPDocumentPlugin.hpp"
#include <core/document/DocumentModel.hpp>

#include <ProcessInterface/Process.hpp>
#include "Document/Constraint/Rack/RackModel.hpp"

#ifdef ISCORE_STATIC_PLUGINS
// This part is somewhat similar to what moc does
// with moc_.. stuff generation.
#include <iscore/tools/NotifyingMap_impl.hpp>
void ignore_template_instantiations_CSPControl()
{
    NotifyingMapInstantiations_T<RackModel>();
    NotifyingMapInstantiations_T<Process>();
}
#endif
CSPControl::CSPControl(iscore::Presenter* pres) :
    iscore::PluginControlInterface {pres, "CSPControl", nullptr}
{
}

iscore::DocumentDelegatePluginModel*CSPControl::loadDocumentPlugin(
        const QString& name,
        const VisitorVariant& var,
        iscore::DocumentModel* model)
{
    // We don't have anything to load; it's easier to just recreate.
    return nullptr;
}

void
CSPControl::on_newDocument(iscore::Document* document)
{
    document->model().addPluginModel(new CSPDocumentPlugin{document->model(), &document->model()});
}

void
CSPControl::on_loadedDocument(iscore::Document* document)
{
    if(auto pluginModel = document->model().pluginModel<CSPDocumentPlugin>())
    {
        pluginModel->reload(document->model());
    }
    else
    {
        on_newDocument(document);
    }
}
