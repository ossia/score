#include <QByteArray>
#include <QCryptographicHash>
#include <QDataStream>
#include <QIODevice>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaType>
#include <QPair>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QtGlobal>
#include <algorithm>
#include <core/application/ApplicationSettings.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentView.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iterator>
#include <stdexcept>
#include <vector>

#include "Document.hpp"
#include "DocumentModel.hpp"
#include <core/command/CommandStack.hpp>
#include <iscore/application/ApplicationComponents.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/model/IdentifiedObject.hpp>

#include <core/presenter/DocumentManager.hpp>
#include <iscore/model/Identifier.hpp>

namespace iscore
{
QByteArray Document::saveDocumentModelAsByteArray()
{
  // TODO refactor this

  QByteArray arr;

  Serializer<DataStream> s{&arr};

  s.readFrom(model().id());
  TSerializer<DataStream, IdentifiedObject<DocumentDelegateModel>>::readFrom(s, m_model->modelDelegate());
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

  for (const auto& plugin : model().pluginModels())
  {
    if (auto serializable_plugin
        = dynamic_cast<SerializableDocumentPlugin*>(plugin))
    {
      Serializer<JSONObject> s_before;
      s_before.readFrom(*serializable_plugin);

      Serializer<JSONObject> s_after;
      serializable_plugin->serializeAfterDocument(s_after.toVariant());

      s_before.m_obj["DocumentPostModelPart"] = std::move(s_after.m_obj);
      json_plugins[serializable_plugin->objectName()] = std::move(s_before.m_obj);
    }
  }

  complete["Plugins"] = json_plugins;
  complete["Document"] = saveDocumentModelAsJson();
  complete["Version"]
      = context().app.applicationSettings.saveFormatVersion.value();

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
  QVector<QPair<QByteArray, QByteArray>> documentPluginModels;

  for (const auto& plugin : model().pluginModels())
  {
    if (auto serializable_plugin
        = dynamic_cast<SerializableDocumentPlugin*>(plugin))
    {
      static_assert(iscore::is_object<SerializableDocumentPlugin>::value, "");
      static_assert(
            std::is_same<
              serialization_tag<SerializableDocumentPlugin>::type,
            visitor_abstract_object_tag>::value, "");
      QByteArray arr_before, arr_after;
      Serializer<DataStream> s_before{&arr_before};
      Serializer<DataStream> s_after{&arr_after};
      s_before.readFrom(*serializable_plugin);
      serializable_plugin->serializeAfterDocument(s_after.toVariant());
      documentPluginModels.push_back({std::move(arr_before), std::move(arr_after)});
    }
  }

  writer << docByteArray << documentPluginModels;

  auto hash = QCryptographicHash::hash(
      global, QCryptographicHash::Algorithm::Sha512);
  writer << hash;

  // Indicate in the stack that the current position is saved
  m_commandStack.markCurrentIndexAsSaved();
  return global;
}

// Load document
Document::Document(
    const QVariant& data,
    DocumentDelegateFactory& factory,
    QWidget* parentview,
    QObject* parent)
    : QObject{parent}
    , m_commandStack{*this}
    , m_objectLocker{this}
    , m_context{*this}
{
  std::allocator<DocumentModel> allocator;
  m_model = allocator.allocate(1);
  try
  {
    allocator.construct(m_model, m_context, data, factory, this);
  }
  catch (...)
  {
    allocator.deallocate(m_model, 1);
    throw;
  }

  m_view = new DocumentView{factory, *this, parentview};
  m_presenter = new DocumentPresenter{factory, *m_model, *m_view, this};
  init();
}

void DocumentModel::loadDocumentAsByteArray(
    iscore::DocumentContext& ctx,
    const QByteArray& data,
    DocumentDelegateFactory& fact)
{
  // Deserialize the first parts
  QByteArray doc;
  QVector<QPair<QByteArray, QByteArray>> documentPluginModels;
  QByteArray hash;

  QDataStream wr{data};
  wr >> doc >> documentPluginModels >> hash;

  // Perform hash verification
  QByteArray verif_arr;
  QDataStream writer(&verif_arr, QIODevice::WriteOnly);
  writer << doc << documentPluginModels;
  if (QCryptographicHash::hash(
          verif_arr, QCryptographicHash::Algorithm::Sha512)
      != hash)
  {
    throw std::runtime_error("Invalid file.");
  }

  // Set the id

  DataStream::Deserializer doc_writer{doc};
  {
    Id<DocumentModel> doc_id;
    doc_writer.writeTo(doc_id);

    if (ossia::any_of(ctx.app.documents.documents(), [=](auto doc) {
          return doc->id() == doc_id;
        }))
      throw std::runtime_error(
          tr("The document is already loaded").toStdString());

    this->setId(std::move(doc_id));
  }

  // Note : this *has* to be in this order, because
  // the plugin models might put some data in the
  // document that requires the plugin models to be loaded
  // in order to be deserialized. (e.g. the groups for the network)
  // First load the plugin models

  const auto plug_n = documentPluginModels.size();

  auto& plugin_factories
      = ctx.app.interfaces<DocumentPluginFactoryList>();
  std::vector<iscore::DocumentPlugin*> docs(plug_n, nullptr);

  for(int i = 0; i < plug_n; i++)
  {
    const auto& plugin_raw = documentPluginModels[i];

    DataStream::Deserializer plug_writer{plugin_raw.first};
    auto plug
        = deserialize_interface(plugin_factories, plug_writer, ctx, this);

    docs[i] = plug;
    if (plug)
    {
      this->addPluginModel(plug);
    }
    else
    {
      ISCORE_TODO;
    }
  }

  // Load the document model
  m_model = fact.load(doc_writer.toVariant(), ctx, this);

  for(int i = 0; i < plug_n; i++)
  {
    if(auto plug = dynamic_cast<iscore::SerializableDocumentPlugin*>(docs[i]))
    {
      DataStream::Deserializer plug_writer{documentPluginModels[i].second};
      plug->reloadAfterDocument(plug_writer.toVariant());
    }
  }
}

void DocumentModel::loadDocumentAsJson(
    iscore::DocumentContext& ctx,
    const QJsonObject& json,
    DocumentDelegateFactory& fact)
{
  const auto& doc_obj = json.find("Document");
  if (doc_obj == json.end())
    throw std::runtime_error(tr("Invalid document").toStdString());

  const auto& doc = (*doc_obj).toObject();
  auto doc_id = fromJsonValue<Id<DocumentModel>>(doc["DocumentId"]);

  if (ossia::any_of(ctx.app.documents.documents(), [=](auto doc) {
        return doc->id() == doc_id;
      }))
    throw std::runtime_error(
        tr("The document is already loaded").toStdString());

  this->setId(doc_id);

  iscore::hash_map<iscore::SerializableDocumentPlugin*, QJsonObject> docs;
  // Load the plug-in models
  auto json_plugins = json["Plugins"].toObject();
  auto& plugin_factories
      = ctx.app.interfaces<DocumentPluginFactoryList>();
  Foreach(json_plugins.keys(), [&](const auto& key) {
    JSONObject::Deserializer plug_writer{json_plugins[key].toObject()};
    auto plug
        = deserialize_interface(plugin_factories, plug_writer, ctx, this);

    if (plug)
    {
      if(auto ser = dynamic_cast<iscore::SerializableDocumentPlugin*>(plug))
      {
        auto it = plug_writer.m_obj.find("DocumentPostModelPart");
        if((it != plug_writer.m_obj.end()) && it->isObject())
        {
          docs.insert({ser, it->toObject()});
        }
      }
      this->addPluginModel(plug);
    }
    else
    {
      ISCORE_TODO;
    }
  });

  // Load the model
  JSONObject::Deserializer doc_writer{doc};
  m_model = fact.load(doc_writer.toVariant(), ctx, this);

  auto it_end = docs.end();
  for(auto it = docs.begin(); it != it_end; ++it)
  {
    JSONObject::Deserializer des{it.value()};
    it->first->reloadAfterDocument(des.toVariant());
  }
}

// Load document model
DocumentModel::DocumentModel(
    iscore::DocumentContext& ctx,
    const QVariant& data,
    DocumentDelegateFactory& fact,
    QObject* parent)
    : IdentifiedObject{Id<DocumentModel>(iscore::id_generator::getFirstId()),
                       "DocumentModel", parent}
{
  using namespace std;

  for (auto& appPlug : ctx.app.applicationPlugins())
  {
    appPlug->on_initDocument(ctx.document);
  }

  try
  {
    if (data.canConvert(QMetaType::QByteArray))
    {
      loadDocumentAsByteArray(ctx, data.toByteArray(), fact);
    }
    else if (data.canConvert(QMetaType::QJsonObject))
    {
      loadDocumentAsJson(ctx, data.toJsonObject(), fact);
    }
    else
    {
      ISCORE_ABORT;
    }
  }
  catch (...)
  {
    // In case of exception, we just clear the container so that some plug-in
    // does not try to access
    // a "dead" plug-in due to the deletion order of QObject (e.g. calling
    // findPlugin in a dtor).
    m_pluginModels.clear();
    throw;
  }
}
}
