#include <core/document/DocumentModel.hpp>
#include <plugin_interface/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <plugin_interface/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <plugin_interface/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <plugin_interface/panel/PanelModelInterface.hpp>
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
