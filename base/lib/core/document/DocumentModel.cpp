#include <core/document/DocumentModel.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/plugins/panel/PanelModelInterface.hpp>
#include <exception>


using namespace iscore;

DocumentModel::DocumentModel(DocumentDelegateFactoryInterface* fact,
                             QObject* parent) :
    IdentifiedObject {id_type<DocumentModel>(getNextId()), "DocumentModel", parent},
    m_model{fact->makeModel(this)}
{
}

DocumentModel::DocumentModel(QVariant data,
                             DocumentDelegateFactoryInterface* fact,
                             QObject* parent) :
    IdentifiedObject {id_type<DocumentModel>(getNextId()), "DocumentModel", parent}
{
    if(data.canConvert(QMetaType::QByteArray))
    {
        auto full = data.toByteArray();
        QByteArray doc;
        QVector<QPair<QString, QByteArray>> panelModels;
        QDataStream wr(full);
        wr >> doc >> panelModels;
        // TODO check hash


        m_model = fact->makeModel(this, doc);
    }
    else if(data.canConvert(QMetaType::QJsonObject))
    {
        auto json = data.toJsonObject();
        m_model = fact->makeModel(this, json["Scenario"].toObject());
    }
    else
    {
        qFatal("Could not load DocumentModel");
        return;
    }
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
