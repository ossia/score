#include "CSPApplicationPlugin.hpp"
#include "CSPDocumentPlugin.hpp"
#include <core/document/DocumentModel.hpp>

#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>

#if defined(ISCORE_STATIC_PLUGINS) && defined(ISCORE_COMPILER_IS_AppleClang)
// This part is somewhat similar to what moc does
// with moc_.. stuff generation.
#include <iscore/tools/NotifyingMap_impl.hpp>
void ignore_template_instantiations_CSPApplicationPlugin()
{
    NotifyingMapInstantiations_T<RackModel>();
    NotifyingMapInstantiations_T<Process>();
}
#endif
CSPApplicationPlugin::CSPApplicationPlugin(iscore::Presenter* pres) :
    iscore::GUIApplicationContextPlugin {pres, "CSPApplicationPlugin", nullptr}
{
}

iscore::DocumentDelegatePluginModel*CSPApplicationPlugin::loadDocumentPlugin(
        const QString& name,
        const VisitorVariant& var,
        iscore::DocumentModel* model)
{
    // We don't have anything to load; it's easier to just recreate.
    return nullptr;
}

void
CSPApplicationPlugin::on_newDocument(iscore::Document* document)
{
    document->model().addPluginModel(new CSPDocumentPlugin{document->model(), &document->model()});
}

void
CSPApplicationPlugin::on_loadedDocument(iscore::Document* document)
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
