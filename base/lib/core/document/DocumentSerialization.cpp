// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Document.hpp"
#include "DocumentModel.hpp"

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
#include <core/command/CommandStack.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentView.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <iterator>
#include <score/application/ApplicationComponents.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/std/Optional.hpp>
#include <stdexcept>
#include <vector>

namespace score
{
QByteArray Document::saveDocumentModelAsByteArray()
{
  // TODO refactor this
  QByteArray arr;

  DataStream::Serializer s{&arr};

  TSerializer<DataStream, IdentifiedObject<DocumentDelegateModel>>::readFrom(
      s, m_model->modelDelegate());
  m_model->modelDelegate().serialize(s.toVariant());
  return arr;
}

QJsonObject Document::saveDocumentModelAsJson()
{
  JSONObject::Serializer s;
  TSerializer<JSONObject, IdentifiedObject<DocumentDelegateModel>>::readFrom(
      s, m_model->modelDelegate());
  m_model->modelDelegate().serialize(s.toVariant());
  return s.obj;
}

QJsonObject Document::saveAsJson()
{
  using namespace std;
  QJsonObject complete, json_plugins;

  for (const auto& plugin : model().pluginModels())
  {
    if (auto serializable_plugin
        = qobject_cast<SerializableDocumentPlugin*>(plugin))
    {
      JSONObject::Serializer s_before;
      s_before.readFrom(*serializable_plugin);

      JSONObject::Serializer s_after;
      serializable_plugin->serializeAfterDocument(s_after.toVariant());

      s_before.obj["DocumentPostModelPart"] = std::move(s_after.obj);
      json_plugins[serializable_plugin->objectName()]
          = std::move(s_before.obj);
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
        = qobject_cast<SerializableDocumentPlugin*>(plugin))
    {
      static_assert(
          is_identified_object<SerializableDocumentPlugin>::value, "");
      static_assert(
          std::is_same<
              serialization_tag<SerializableDocumentPlugin>::type,
              visitor_abstract_object_tag>::value,
          "");
      QByteArray arr_before, arr_after;
      DataStream::Serializer s_before{&arr_before};
      DataStream::Serializer s_after{&arr_after};
      s_before.readFrom(*serializable_plugin);
      serializable_plugin->serializeAfterDocument(s_after.toVariant());
      documentPluginModels.push_back(
          {std::move(arr_before), std::move(arr_after)});
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
    const QString& name,
    const QVariant& data,
    DocumentDelegateFactory& factory,
    QWidget* parentview,
    QObject* parent)
    : QObject{parent}
    , m_metadata{name}
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

  if (parentview)
  {
    m_view = new DocumentView{factory, *this, parentview};
    m_presenter
        = new DocumentPresenter{m_context, factory, *m_model, *m_view, this};
  }
  init();
}

Document::Document(
    const QString& name,
    const QVariant& data,
    DocumentDelegateFactory& factory,
    QObject* parent)
    : QObject{parent}
    , m_metadata{name}
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
}

void DocumentModel::loadDocumentAsByteArray(
    score::DocumentContext& ctx,
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
  this->setId(getStrongId(ctx.app.documents.documents()));

  // Note : this *has* to be in this order, because
  // the plugin models might put some data in the
  // document that requires the plugin models to be loaded
  // in order to be deserialized. (e.g. the groups for the network)
  // First load the plugin models

  const auto plug_n = documentPluginModels.size();

  auto& plugin_factories = ctx.app.interfaces<DocumentPluginFactoryList>();
  std::vector<score::DocumentPlugin*> docs(plug_n, nullptr);

  for (int i = 0; i < plug_n; i++)
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
      SCORE_TODO;
    }
  }

  // Load the document model
  fact.load(doc_writer.toVariant(), ctx, m_model, this);

  for (int i = 0; i < plug_n; i++)
  {
    if (auto plug = qobject_cast<score::SerializableDocumentPlugin*>(docs[i]))
    {
      DataStream::Deserializer plug_writer{documentPluginModels[i].second};
      plug->reloadAfterDocument(plug_writer.toVariant());
    }
  }
}

void DocumentModel::loadDocumentAsJson(
    score::DocumentContext& ctx,
    const QJsonObject& json,
    DocumentDelegateFactory& fact)
{
  const auto& doc_obj = json.find("Document");
  if (doc_obj == json.end())
    throw std::runtime_error(tr("Invalid document").toStdString());

  const auto& doc = (*doc_obj).toObject();
  this->setId(getStrongId(ctx.app.documents.documents()));

  score::hash_map<score::SerializableDocumentPlugin*, QJsonObject> docs;
  // Load the plug-in models
  auto json_plugins = json["Plugins"].toObject();
  auto& plugin_factories = ctx.app.interfaces<DocumentPluginFactoryList>();
  Foreach(json_plugins.keys(), [&](const auto& key) {
    JSONObject::Deserializer plug_writer{json_plugins[key].toObject()};
    auto plug
        = deserialize_interface(plugin_factories, plug_writer, ctx, this);

    if (plug)
    {
      if (auto ser = qobject_cast<score::SerializableDocumentPlugin*>(plug))
      {
        auto it = plug_writer.obj.find("DocumentPostModelPart");
        if ((it != plug_writer.obj.end()) && it->isObject())
        {
          docs.insert({ser, it->toObject()});
        }
      }
      this->addPluginModel(plug);
    }
    else
    {
      SCORE_TODO;
    }
  });

  // Load the model
  JSONObject::Deserializer doc_writer{doc};
  fact.load(doc_writer.toVariant(), ctx, m_model, this);

  auto it_end = docs.end();
  for (auto it = docs.begin(); it != it_end; ++it)
  {
    JSONObject::Deserializer des{it.value()};
    it->first->reloadAfterDocument(des.toVariant());
  }
}

// Load document model
DocumentModel::DocumentModel(
    score::DocumentContext& ctx,
    const QVariant& data,
    DocumentDelegateFactory& fact,
    QObject* parent)
    : IdentifiedObject{Id<DocumentModel>(score::id_generator::getFirstId()),
                       "DocumentModel", parent}
{
  using namespace std;

  for (auto& appPlug : ctx.app.guiApplicationPlugins())
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
      SCORE_ABORT;
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
