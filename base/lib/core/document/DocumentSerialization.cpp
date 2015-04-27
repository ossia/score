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
    QByteArray arr;
    Serializer<DataStream> s{&arr};
    m_model->modelDelegate()->serialize(s.toVariant());
    return arr;
}

QJsonObject Document::saveDocumentModelAsJson()
{
    Serializer<JSONObject> s;
    m_model->modelDelegate()->serialize(s.toVariant());
    return s.m_obj;
}

QJsonObject Document::saveAsJson()
{
    QJsonObject complete;
    for(const auto& panel : model()->panels())
    {
        Serializer<JSONObject> s;
        panel->serialize(s.toVariant());
        complete[panel->objectName()] = s.m_obj;
    }

    for(const auto& plugin : model()->pluginModels())
    {
        Serializer<JSONObject> s;
        plugin->serialize(s.toVariant());
        complete[plugin->objectName()] = s.m_obj;
    }

    complete["Document"] = saveDocumentModelAsJson();

    // Indicate in the stack that the current position is saved
    m_commandStack.markCurrentIndexAsSaved();
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
    {
        QByteArray arr;
        Serializer<DataStream> s{&arr};
        panel->serialize(s.toVariant());

        return QPair<QString, QByteArray>{
            panel->objectName(),
            arr};
    });

    // Save the document plug-ins
    QVector<QPair<QString, QByteArray>> documentPluginModels;
    std::transform(begin(model()->pluginModels()),
                   end(model()->pluginModels()),
                   std::back_inserter(documentPluginModels),
                   [] (DocumentDelegatePluginModel* plugin)
    {
        QByteArray arr;
        Serializer<DataStream> s{&arr};
        plugin->serialize(s.toVariant());

        return QPair<QString, QByteArray>{
            plugin->objectName(),
            arr};
    });

    writer << docByteArray << panelModels << documentPluginModels;

    auto hash = QCryptographicHash::hash(global, QCryptographicHash::Algorithm::Sha512);
    writer << hash;

    // Indicate in the stack that the current position is saved
    m_commandStack.markCurrentIndexAsSaved();
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
        // Deserialize the first parts
        QByteArray doc;
        QVector<QPair<QString, QByteArray>> panelModels, documentPluginModels;
        QByteArray hash;

        QDataStream wr{data.toByteArray()};
        wr >> doc >> panelModels >> documentPluginModels >> hash;

        // Perform hash verification
        QByteArray verif_arr;
        QDataStream writer(&verif_arr, QIODevice::WriteOnly);
        writer << doc << panelModels << documentPluginModels;
        Q_ASSERT(QCryptographicHash::hash(verif_arr, QCryptographicHash::Algorithm::Sha512) == hash);


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

            // Note : here we handle the case where the plug-in is not able to
            // load data; generally because it is not useful (e.g. undo panel model...)

            DataStream::Deserializer panel_writer{panel.second};
            if(auto pnl = factory->makeModel(panel_writer.toVariant(), this))
                addPanel(pnl);
            else
                addPanel(factory->makeModel(this));
        }

        qDebug() << Q_FUNC_INFO << "TODO Load plugin models";

        // Load the document model

        DataStream::Deserializer doc_writer{doc};
        m_model = fact->makeModel(doc_writer.toVariant(), this);
    }
    else if(data.canConvert(QMetaType::QJsonObject))
    {
        QJsonObject json = data.toJsonObject();

        auto factories = iscore::IPresenter::panelFactories();
        for(const auto& key : json.keys())
        {
            auto factory_it = find_if(begin(factories),
                                   end(factories),
                                   [&] (iscore::PanelFactoryInterface* fact) { return fact->name() == key; });
            if(factory_it != end(factories))
            {
                auto factory = *factory_it;
                // Note : here we handle the case where the plug-in is not able to
                // load data; generally because it is not useful (e.g. undo panel model...)

                JSONObject::Deserializer panel_writer{json[key].toObject()};
                if(auto pnl = factory->makeModel(panel_writer.toVariant(), this))
                    addPanel(pnl);
                else
                    addPanel(factory->makeModel(this));
            }
        }

        qDebug() << Q_FUNC_INFO << "TODO Load plugin models";

        JSONObject::Deserializer doc_writer{json["Document"].toObject()};
        m_model = fact->makeModel(doc_writer.toVariant(), this);
    }
    else
    {
        qFatal("Could not load DocumentModel");
        return;
    }
}
