#include "Document.hpp"
#include "DocumentModel.hpp"

#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

#include <iscore/plugins/panel/PanelFactoryInterface.hpp>

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
    for(auto panel : model()->panels())
    {
        complete[panel->objectName()] = panel->toJson();
    }

    for(auto plugin : model()->pluginModels())
    {
        complete[plugin->objectName()] = plugin->toJson();
    }

    complete["Document"] = saveDocumentModelAsJson();

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

#include <iscore/presenter/PresenterInterface.hpp>
// Document model
DocumentModel::DocumentModel(const QVariant& data,
                             DocumentDelegateFactoryInterface* fact,
                             QObject* parent) :
    IdentifiedObject {id_type<DocumentModel>(getNextId()), "DocumentModel", parent}
{
    using namespace std;
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

        qDebug() << Q_FUNC_INFO << "TODO Load panel models and plugin models";

        // Note : this *has* to be in this order, because
        // the plugin models might put some data in the
        // document that requires the plugin models to be loaded
        // in order to be deserialized.
        auto factories = iscore::IPresenter::panelFactories();
        for(const auto& panel : panelModels)
        {
            auto factory = *find_if(begin(factories),
                                   end(factories),
                                   [&] (iscore::PanelFactoryInterface* fact) { return fact->name() == panel.first; });
            if(auto pnl = factory->makeModel(panel.second, this))
                addPanel(pnl);
            else
                addPanel(factory->makeModel(this));
        }

        // Load the document model
        m_model = fact->makeModel(doc, this);
    }
    else if(data.canConvert(QMetaType::QJsonObject))
    {
        auto json = data.toJsonObject();
        qDebug() << Q_FUNC_INFO << "TODO Load panel models and plugin models";

        m_model = fact->makeModel(json["Document"].toObject(), this);
    }
    else
    {
        qFatal("Could not load DocumentModel");
        return;
    }
}
