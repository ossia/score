#include <core/document/DocumentModel.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

#include <iscore/selection/Selection.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

using namespace iscore;

DocumentModel::DocumentModel(
        const Id<DocumentModel>& id,
        DocumentDelegateFactoryInterface* fact,
        QObject* parent) :
    IdentifiedObject {id, "DocumentModel", parent},
    m_model{fact->makeModel(this)}
{
}

void DocumentModel::addPanel(PanelModel *m)
{
    m_panelModels.push_back(m);
}




void DocumentModel::addPluginModel(DocumentPluginModel *m)
{
    m->setParent(this);
    m_pluginModels.push_back(m);
    emit pluginModelsChanged();
}



void DocumentModel::setNewSelection(const Selection& s)
{
    m_model->setNewSelection(s);
}

QString DocumentModel::docFileName() const
{
    return m_docFileName;
}

void DocumentModel::setDocFileName(const QString &docFileName)
{
    m_docFileName = docFileName;
    emit fileNameChanged(docFileName);
}
