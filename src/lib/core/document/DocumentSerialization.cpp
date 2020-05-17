// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Document.hpp"
#include "DocumentModel.hpp"

#include <score/application/ApplicationComponents.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPluginCreator.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/widgets/MessageBox.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/command/CommandStack.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentView.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QApplication>
#include <QByteArray>
#include <QCryptographicHash>
#include <QDataStream>
#include <QFile>
#include <QIODevice>
#include <QMetaType>
#include <QPair>
#include <QString>
#include <QVariant>
#include <QVector>

#include <fmt/format.h>

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

void Document::saveDocumentModelAsJson(JSONObject::Serializer& writer)
{
  writer.stream.StartObject();

  TSerializer<JSONObject, IdentifiedObject<DocumentDelegateModel>>::readFrom(
      writer, m_model->modelDelegate());
  m_model->modelDelegate().serialize(writer.toVariant());

  writer.stream.EndObject();
}

void Document::saveAsJson(JSONObject::Serializer& writer)
{
  using namespace std;
  writer.stream.StartObject();
  writer.stream.Key("Document");
  saveDocumentModelAsJson(writer);

  rapidjson::Value complete, json_plugins;

  writer.stream.Key("Plugins");
  writer.stream.StartArray();
  for (const auto& plugin : model().pluginModels())
  {
    if (auto serializable_plugin = qobject_cast<SerializableDocumentPlugin*>(plugin))
    {
      writer.readFrom(*serializable_plugin);
    }
  }
  writer.stream.EndArray();

  writer.stream.Key("Version");
  writer.stream.Int(context().app.applicationSettings.saveFormatVersion.value());

  writer.stream.EndObject();
  // Indicate in the stack that the current position is saved
  m_commandStack.markCurrentIndexAsSaved();
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
    if (auto serializable_plugin = qobject_cast<SerializableDocumentPlugin*>(plugin))
    {
      static_assert(is_identified_object<SerializableDocumentPlugin>::value, "");
      static_assert(
          std::is_same<
              serialization_tag<SerializableDocumentPlugin>::type,
              visitor_abstract_object_tag>::value,
          "");
      QByteArray arr_before, arr_after;
      DataStream::Serializer s_before{&arr_before};
      s_before.readFrom(*serializable_plugin);
      documentPluginModels.push_back({std::move(arr_before), std::move(arr_after)});
    }
  }

  writer << docByteArray << documentPluginModels;

  auto hash = QCryptographicHash::hash(global, QCryptographicHash::Algorithm::Sha512);
  writer << hash;

  // Indicate in the stack that the current position is saved
  m_commandStack.markCurrentIndexAsSaved();
  return global;
}

// Load document
Document::Document(
    const QString& fileName,
    DocumentDelegateFactory& factory,
    QWidget* parentview,
    QObject* parent)
    : QObject{parent}
    , m_metadata{fileName}
    , m_commandStack{*this}
    , m_objectLocker{this}
    , m_context{*this}
{
  loadModel(fileName, factory);

  if (parentview)
  {
    m_view = new DocumentView{factory, *this, parentview};
    m_presenter = new DocumentPresenter{m_context, factory, *m_model, *m_view, this};
  }
  init();
}

// Restore
Document::Document(
    const QString& fileName,
    const QByteArray& data,
    DocumentDelegateFactory& factory,
    QWidget* parentview,
    QObject* parent)
    : QObject{parent}
    , m_metadata{fileName}
    , m_commandStack{*this}
    , m_objectLocker{this}
    , m_context{*this}
{
  restoreModel(data, factory);

  if (parentview)
  {
    m_view = new DocumentView{factory, *this, parentview};
    m_presenter = new DocumentPresenter{m_context, factory, *m_model, *m_view, this};
  }
  init();
}

void Document::restoreModel(const QByteArray& data, DocumentDelegateFactory& factory)
{
  std::allocator<DocumentModel> allocator;
  m_model = allocator.allocate(1);
  new (m_model) DocumentModel(this);

  for (auto& appPlug : m_context.app.guiApplicationPlugins())
  {
    appPlug->on_initDocument(*this);
  }

  m_model->loadDocumentAsByteArray(m_context, data, factory);
}

void Document::loadModel(const QString& fileName, DocumentDelegateFactory& factory)
{
  std::allocator<DocumentModel> allocator;
  m_model = allocator.allocate(1);
  new (m_model) DocumentModel(this);

  for (auto& appPlug : m_context.app.guiApplicationPlugins())
  {
    appPlug->on_initDocument(*this);
  }

  if (fileName.indexOf(".scorebin") != -1)
  {
    QFile f(fileName);
    f.open(QIODevice::ReadOnly);
    auto data = f.readAll();
    SCORE_ASSERT(!data.isEmpty());

    m_model->loadDocumentAsByteArray(m_context, data, factory);
  }
  else if (fileName.indexOf(".score") != -1)
  {
    QFile f(fileName);
    f.open(QIODevice::ReadOnly);
    auto data = f.readAll();
    SCORE_ASSERT(!data.isEmpty());

    auto doc = readJson(data);
    bool ok = DocumentManager::checkAndUpdateJson(doc, m_context.app);
    if (!ok)
    {
      score::warning(
          qApp->activeWindow(),
          tr("Warning"),
          tr("There is probably something wrong with the loaded file."));
    }
    m_model->loadDocumentAsJson(m_context, doc, factory);
  }
  else
  {
    // Create a blank document
    factory.make(m_context, m_model->m_model, m_model);
  }
}

Document::Document(const QString& fileName, DocumentDelegateFactory& factory, QObject* parent)
    : QObject{parent}
    , m_metadata{fileName}
    , m_commandStack{*this}
    , m_objectLocker{this}
    , m_context{*this}
{
  loadModel(fileName, factory);
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
  if (QCryptographicHash::hash(verif_arr, QCryptographicHash::Algorithm::Sha512) != hash)
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
    auto plug = deserialize_interface(plugin_factories, plug_writer, ctx, this);

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
}

void DocumentModel::loadDocumentAsJson(
    score::DocumentContext& ctx,
    const rapidjson::Value& json,
    DocumentDelegateFactory& fact)
{
  const auto& doc_obj = json.FindMember("Document");
  if (doc_obj == json.MemberEnd())
    throw std::runtime_error(tr("Invalid document").toStdString());

  const auto& doc = (*doc_obj).value;
  this->setId(getStrongId(ctx.app.documents.documents()));

  // Load the plug-in models
  auto json_plugins = json["Plugins"].GetArray();
  auto& plugin_factories = ctx.app.interfaces<DocumentPluginFactoryList>();
  for (const auto& plugin : json_plugins)
  {
    JSONObject::Deserializer plug_writer{plugin};
    auto plug = deserialize_interface(plugin_factories, plug_writer, ctx, this);

    if (plug)
    {
      this->addPluginModel(plug);
    }
    else
    {
      SCORE_TODO;
    }
  }

  // Load the model
  JSONObject::Deserializer doc_writer{doc};
  fact.load(doc_writer.toVariant(), ctx, m_model, this);
}

// Load document model
DocumentModel::DocumentModel(QObject* parent)
    : IdentifiedObject{
        Id<DocumentModel>(score::id_generator::getFirstId()),
        "DocumentModel",
        parent}
{
}

}
