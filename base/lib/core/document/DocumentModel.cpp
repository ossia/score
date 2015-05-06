#include <core/document/DocumentModel.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/plugins/panel/PanelModel.hpp>

using namespace iscore;

DocumentModel::DocumentModel(DocumentDelegateFactoryInterface* fact,
                             QObject* parent) :
    IdentifiedObject {id_type<DocumentModel>(getNextId()), "DocumentModel", parent},
    m_model{fact->makeModel(this)}
{
}

void DocumentModel::addPanel(PanelModel *m)
{
    m_panelModels.append(m);
}


PanelModel* DocumentModel::panel(const QString &name) const
{
    using namespace std;
    auto it = find_if(begin(m_panelModels),
                      end(m_panelModels),
                      [&](PanelModel * pm)
    {
        return pm->objectName() == name;
    });

    return it != end(m_panelModels) ? *it : nullptr;
}

void DocumentModel::addPluginModel(DocumentDelegatePluginModel *m)
{
    m->setParent(this);
    m_pluginModels.append(m);
    emit pluginModelsChanged();
}

DocumentDelegatePluginModel*DocumentModel::pluginModel(const QString &name) const
{
    using namespace std;
    auto it = find_if(begin(m_pluginModels),
                      end(m_pluginModels),
                      [&](DocumentDelegatePluginModel * pm)
    {
        return pm->objectName() == name;
    });

    return it != end(m_pluginModels) ? *it : nullptr;
}

void DocumentModel::setNewSelection(const Selection& s)
{
    m_model->setNewSelection(s);
}
