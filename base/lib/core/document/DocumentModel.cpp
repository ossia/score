#include <core/document/DocumentModel.hpp>
#include <interface/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <interface/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <interface/panel/PanelModelInterface.hpp>
#include <exception>


using namespace iscore;

DocumentModel::DocumentModel(DocumentDelegateFactoryInterface* fact,
                             QObject* parent) :
    NamedObject {"DocumentModel", parent},
    m_model{fact->makeModel(this)}
{

}

DocumentModel::DocumentModel(const QByteArray& data,
                             DocumentDelegateFactoryInterface* fact,
                             QObject* parent) :
    NamedObject {"DocumentModel", parent},
    m_model{fact->makeModel(this, data)}
{

}

PanelModelInterface* DocumentModel::panel(QString name) const
{
    using namespace std;
    auto it = find_if(begin(m_panelModels),
                      end(m_panelModels),
                      [&](PanelModelInterface * pm)
    {
        return pm->objectName() == name;
    });

    return it != end(m_panelModels) ? *it : nullptr;
}

DocumentDelegatePluginModel*DocumentModel::pluginModel(QString name) const
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
