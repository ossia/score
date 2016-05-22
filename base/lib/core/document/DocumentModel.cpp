#include <core/document/DocumentModel.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

#include <iscore/selection/Selection.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace iscore
{
DocumentModel::DocumentModel(
        const Id<DocumentModel>& id,
        const iscore::DocumentContext& ctx,
        DocumentDelegateFactory& fact,
        QObject* parent) :
    IdentifiedObject {id, "DocumentModel", parent},
    m_model{fact.makeModel(ctx, this)}
{
}

DocumentModel::~DocumentModel()
{
    delete m_model;
    // We remove the plug-ins first.
    for(auto plug : m_pluginModels)
    {
        delete plug;
    }
}

void DocumentModel::addPluginModel(DocumentPlugin *m)
{
    m->setParent(this);
    m_pluginModels.push_back(m);
    emit pluginModelsChanged();
}

void DocumentModel::setNewSelection(const Selection& s)
{
    m_model->setNewSelection(s);
}
}
