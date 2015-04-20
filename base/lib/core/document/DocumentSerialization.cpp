#include "Document.hpp"
#include "DocumentModel.hpp"

#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

#include <iscore/plugins/panel/PanelModelInterface.hpp>
#include <QCryptographicHash>
using namespace iscore;
// Document stuff

QByteArray Document::saveDocumentModelAsByteArray()
{
    return m_model->modelDelegate()->toByteArray();
}

QJsonObject Document::saveDocumentModelAsJson()
{
    return m_model->modelDelegate()->toJson();
}


QJsonObject Document::savePanelAsJson(const QString& panel)
{
    return m_model->modelDelegate()->toJson();
}

QByteArray Document::savePanelAsByteArray(const QString& panel)
{
    using namespace std;
    auto panelmodel = find_if(begin(model()->panels()),
                              end(model()->panels()),
                              [&] (PanelModelInterface* model) { return model->objectName() == panel;});

    return (*panelmodel)->toByteArray();
}

QJsonObject Document::saveAsJson()
{
    QJsonObject complete;
    complete["Document"] = saveDocumentModelAsJson();
    for(auto panel : model()->panels())
    {
        complete[panel->objectName()] = panel->toJson();
    }

    for(auto plugin : model()->pluginModels())
    {
        complete[plugin->objectName()] = plugin->toJson();
    }

    return complete;
}

QByteArray Document::saveAsByteArray()
{
    using namespace std;
    QByteArray global;
    QDataStream writer(&global, QIODevice::WriteOnly);

    // Save the document
    auto docByteArray = saveDocumentModelAsByteArray();

    // Save the panels
    QVector<QPair<QString, QByteArray>> panelModels;
    std::transform(begin(model()->panels()),
                   end(model()->panels()),
                   std::back_inserter(panelModels),
                   [] (PanelModelInterface* panel)
    { return QPair<QString, QByteArray>{
            panel->objectName(),
            panel->toByteArray()};
    });

    // Save the document plug-ins
    QVector<QPair<QString, QByteArray>> documentPluginModels;
    std::transform(begin(model()->pluginModels()),
                   end(model()->pluginModels()),
                   std::back_inserter(documentPluginModels),
                   [] (DocumentDelegatePluginModel* plugin)
    {  return QPair<QString, QByteArray>{
            plugin->objectName(),
            plugin->toByteArray()};
    });

    writer << docByteArray << panelModels << documentPluginModels;

    auto hash = QCryptographicHash::hash(global, QCryptographicHash::Algorithm::Sha512);
    writer << hash;

    return global;
}


// Document model
DocumentModel::DocumentModel(QVariant data,
                             DocumentDelegateFactoryInterface* fact,
                             QObject* parent) :
    IdentifiedObject {id_type<DocumentModel>(getNextId()), "DocumentModel", parent}
{
    if(data.canConvert(QMetaType::QByteArray))
    {
        auto full = data.toByteArray();

        // Deserialize the first parts
        QByteArray doc;
        QVector<QPair<QString, QByteArray>> panelModels, documentPluginModels;
        QByteArray hash;

        QDataStream wr{full};
        wr >> doc >> panelModels >> documentPluginModels >> hash;

        // Perform hash verification
        QByteArray verif_arr;
        QDataStream writer(&verif_arr, QIODevice::WriteOnly);
        writer << doc << panelModels << documentPluginModels;
        Q_ASSERT(QCryptographicHash::hash(verif_arr, QCryptographicHash::Algorithm::Sha512) == hash);

        // Load the data
        m_model = fact->makeModel(this, doc);

        for(auto& panel : panelModels)
        {

        }
    }
    else if(data.canConvert(QMetaType::QJsonObject))
    {
        auto json = data.toJsonObject();
        m_model = fact->makeModel(this, json["Document"].toObject());
    }
    else
    {
        qFatal("Could not load DocumentModel");
        return;
    }
}
