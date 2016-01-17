
#include <boost/optional/optional.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentView.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <QByteArray>
#include <QCryptographicHash>
#include <QDataStream>
#include <QtGlobal>
#include <QIODevice>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaType>
#include <QPair>
#include <QString>
#include <QVariant>
#include <QVector>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <vector>

#include "Document.hpp"
#include "DocumentModel.hpp"
#include <iscore/application/ApplicationComponents.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <core/command/CommandStack.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class QObject;
class QWidget;


namespace iscore
{
QByteArray Document::saveDocumentModelAsByteArray()
{
    QByteArray arr;
    Serializer<DataStream> s{&arr};
    s.readFrom(model().id());
    m_model->modelDelegate().serialize(s.toVariant());
    return arr;
}

QJsonObject Document::saveDocumentModelAsJson()
{
    Serializer<JSONObject> s;
    s.m_obj["DocumentId"] = toJsonValue(model().id());
    m_model->modelDelegate().serialize(s.toVariant());
    return s.m_obj;
}

QJsonObject Document::saveAsJson()
{
    using namespace std;
    QJsonObject complete, json_plugins;

    for(const auto& plugin : model().pluginModels())
    {
        Serializer<JSONObject> s;
        plugin->serialize(s.toVariant());
        json_plugins[plugin->objectName()] = s.m_obj;
    }

    complete["Plugins"] = json_plugins;
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

    // Save the document plug-ins
    QVector<QPair<QString, QByteArray>> documentPluginModels;
    std::transform(begin(model().pluginModels()),
                   end(model().pluginModels()),
                   std::back_inserter(documentPluginModels),
                   [] (DocumentPluginModel* plugin)
    {
        QByteArray arr;
        Serializer<DataStream> s{&arr};
        plugin->serialize(s.toVariant());

        return QPair<QString, QByteArray>{
            plugin->objectName(),
            arr};
    });

    writer << docByteArray << documentPluginModels;

    auto hash = QCryptographicHash::hash(global, QCryptographicHash::Algorithm::Sha512);
    writer << hash;

    // Indicate in the stack that the current position is saved
    m_commandStack.markCurrentIndexAsSaved();
    return global;
}


// Load document
Document::Document(const QVariant& data,
                   DocumentDelegateFactoryInterface* factory,
                   QWidget* parentview,
                   QObject* parent):
    NamedObject {"Document", parent},
    m_objectLocker{this},
    m_context{*this}
{
    std::allocator<DocumentModel> allocator;
    m_model = allocator.allocate(1);
    try
    {
        allocator.construct(m_model, m_context.app, data, factory, this);
    }
    catch(...)
    {
        allocator.deallocate(m_model, 1);
        throw;
    }

    m_view = new DocumentView{factory, *this, parentview};
    m_presenter = new DocumentPresenter{factory,
                    *m_model,
                    *m_view,
                    this};
    init();
}

void DocumentModel::loadDocumentAsByteArray(
        const iscore::ApplicationContext& ctx,
        const QByteArray& data,
        DocumentDelegateFactoryInterface* fact)
{
    // Deserialize the first parts
    QByteArray doc;
    QVector<QPair<QString, QByteArray>> documentPluginModels;
    QByteArray hash;

    QDataStream wr{data};
    wr >> doc >> documentPluginModels >> hash;

    // Perform hash verification
    QByteArray verif_arr;
    QDataStream writer(&verif_arr, QIODevice::WriteOnly);
    writer << doc << documentPluginModels;
    if(QCryptographicHash::hash(verif_arr, QCryptographicHash::Algorithm::Sha512) != hash)
    {
        throw std::runtime_error("Invalid file.");
    }

    // Note : this *has* to be in this order, because
    // the plugin models might put some data in the
    // document that requires the plugin models to be loaded
    // in order to be deserialized. (e.g. the groups for the network)
    // First load the plugin models
    for(const auto& plugin_raw : documentPluginModels)
    {
        DataStream::Deserializer plug_writer{plugin_raw.second};
        for(iscore::GUIApplicationContextPlugin* appPlug : ctx.components.applicationPlugins())
        {
            if(auto loaded_plug = appPlug->loadDocumentPlugin(
                        plugin_raw.first,
                        plug_writer.toVariant(),
                        safe_cast<iscore::Document*>(parent())))
            {
                addPluginModel(loaded_plug);
            }
        }
    }

    // Load the document model
    Id<DocumentModel> docid;

    DataStream::Deserializer doc_writer{doc};
    doc_writer.writeTo(docid);
    this->setId(std::move(docid));
    m_model = fact->loadModel(doc_writer.toVariant(), this);
}

void DocumentModel::loadDocumentAsJson(
        const iscore::ApplicationContext& ctx,
        const QJsonObject& json,
        DocumentDelegateFactoryInterface* fact)
{
    this->setId(fromJsonValue<Id<DocumentModel>>(json["DocumentId"]));

    // Load the plug-in models
    auto json_plugins = json["Plugins"].toObject();
    for(const auto& key : json_plugins.keys())
    {
        JSONObject::Deserializer plug_writer{json_plugins[key].toObject()};
        for(iscore::GUIApplicationContextPlugin* appPlug : ctx.components.applicationPlugins())
        {
            if(auto loaded_plug = appPlug->loadDocumentPlugin(key, plug_writer.toVariant(), safe_cast<iscore::Document*>(parent())))
            {
                addPluginModel(loaded_plug);
            }
        }
    }

    // Load the model
    JSONObject::Deserializer doc_writer{json["Document"].toObject()};
    m_model = fact->loadModel(doc_writer.toVariant(), this);
}

// Load document model
DocumentModel::DocumentModel(
        const iscore::ApplicationContext& ctx,
        const QVariant& data,
        DocumentDelegateFactoryInterface* fact,
        QObject* parent) :
    IdentifiedObject {Id<DocumentModel>(iscore::id_generator::getFirstId()), "DocumentModel", parent}
{
    using namespace std;
    if(data.canConvert(QMetaType::QByteArray))
    {
        loadDocumentAsByteArray(ctx, data.toByteArray(), fact);
    }
    else if(data.canConvert(QMetaType::QJsonObject))
    {
        loadDocumentAsJson(ctx, data.toJsonObject(), fact);
    }
    else
    {
        ISCORE_ABORT;
    }
}
}
