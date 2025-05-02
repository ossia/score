// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DocumentBuilder.hpp"

#include <score/model/Identifier.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsFactory.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPluginCreator.hpp>
#include <score/tools/RandomNameProvider.hpp>
#include <score/widgets/MessageBox.hpp>

#include <core/command/CommandStackSerialization.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentBackupManager.hpp>
#include <core/document/DocumentBackups.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/Window.hpp>

#include <QByteArray>
#include <QDir>
#include <QObject>
#include <QString>
#include <QCoreApplication>

#include <stdexcept>

namespace score
{
DocumentBuilder::DocumentBuilder(QObject* parentPresenter, QWidget* parentView)
    : m_parentPresenter{parentPresenter}
    , m_parentView{parentView}
{
}

SCORE_LIB_BASE_EXPORT
Document* DocumentBuilder::newDocument(
    const score::GUIApplicationContext& ctx, const Id<DocumentModel>& id,
    DocumentDelegateFactory& doctype)
{
  QString docName = "Untitled." + RandomNameProvider::generateShortRandomName();

  // FIXME we can't access Library::Settings::Model here :'(
#if !defined(__EMSCRIPTEN__)
  QSettings set;
  if(auto library = set.value("Library/RootPath").toString(); QDir{library}.exists())
  {
    auto templateDocument = QString{"%1/default.score"}.arg(library);
    if(QFile::exists(templateDocument))
    {
      try
      {
        auto doc = loadDocument(ctx, templateDocument, doctype);
        doc->metadata().setFileName(docName);
        //doc->metadata().set({});
        doc->model().setId(id);
        // TODO cables ?!
        return doc;
      }
      catch(...)
      {
      }
    }
  }
#endif
  auto doc = new Document{docName, id, doctype, m_parentView, m_parentPresenter};

  for(auto& projectsettings : ctx.interfaces<DocumentPluginFactoryList>())
  {
    if(auto fact = dynamic_cast<ProjectSettingsFactory*>(&projectsettings))
      doc->model().addPluginModel(fact->makeModel(doc->context(), &doc->model()));
  }

  for(auto& appPlug : ctx.guiApplicationPlugins())
  {
    appPlug->on_newDocument(*doc);
  }

  for(auto& appPlug : ctx.guiApplicationPlugins())
  {
    appPlug->on_createdDocument(*doc);
  }

  return doc;
}

SCORE_LIB_BASE_EXPORT
Document* DocumentBuilder::loadDocument(
    const score::GUIApplicationContext& ctx, QString filename,
    DocumentDelegateFactory& doctype)
{
  Document* doc = nullptr;
  auto& doclist = ctx.documents.documents();
  try
  {
    doc = new Document{filename, doctype, m_parentView, m_parentPresenter};
    for(auto& appPlug : ctx.guiApplicationPlugins())
    {
      appPlug->on_loadedDocument(*doc);
    }

    for(auto& appPlug : ctx.guiApplicationPlugins())
    {
      appPlug->on_createdDocument(*doc);
    }

    doclist.push_back(doc);

    return doc;
  }
  catch(std::runtime_error& e)
  {
    if(m_parentView)
      score::warning(m_parentView, QObject::tr("Error"), e.what());
    else
      qDebug() << "Error while loading: " << e.what();

    if(!doclist.empty() && doclist.back() == doc)
      doclist.pop_back();

    delete doc;
    return nullptr;
  }
}

SCORE_LIB_BASE_EXPORT
Document* DocumentBuilder::loadDocument(
    const score::GUIApplicationContext& ctx, QString filename, QByteArray data,
    SerializationIdentifier format, DocumentDelegateFactory& doctype)
{
  Document* doc = nullptr;
  auto& doclist = ctx.documents.documents();
  try
  {
    doc = new Document{filename, data, format, doctype, m_parentView, m_parentPresenter};
    for(auto& appPlug : ctx.guiApplicationPlugins())
    {
      appPlug->on_loadedDocument(*doc);
    }

    for(auto& appPlug : ctx.guiApplicationPlugins())
    {
      appPlug->on_createdDocument(*doc);
    }

    doclist.push_back(doc);

    return doc;
  }
  catch(std::runtime_error& e)
  {
    if(m_parentView)
      score::warning(m_parentView, QObject::tr("Error"), e.what());
    else
      qDebug() << "Error while loading: " << e.what();

    if(!doclist.empty() && doclist.back() == doc)
      doclist.pop_back();

    delete doc;
    return nullptr;
  }
}

SCORE_LIB_BASE_EXPORT
Document* DocumentBuilder::restoreDocument(
    const score::GUIApplicationContext& ctx, const score::RestorableDocument& restore,
    DocumentDelegateFactory& doctype)
{
  Document* doc = nullptr;
  auto& doclist = ctx.documents.documents();
  try
  {
    // Restoring behaves just like loading : we reload what was loaded
    // (potentially a blank document which is saved at the beginning, once
    // every plug-in has been loaded)
    doc = new Document{restore, doctype, m_parentView, m_parentPresenter};
    for(auto& appPlug : ctx.guiApplicationPlugins())
    {
      appPlug->on_loadedDocument(*doc);
    }

    for(auto& appPlug : ctx.guiApplicationPlugins())
    {
      appPlug->on_createdDocument(*doc);
    }

    doclist.push_back(doc);

    // We restore the pre-crash command stack.
    DataStream::Deserializer writer(restore.commands);
    loadCommandStack(
        ctx.components, writer, doc->commandStack(), [doc](score::Command* cmd) {
      try
      {
        qDebug() << ".. replaying: " << cmd->key().toString().c_str()
                 << cmd->description();
        cmd->redo(doc->context());
        QCoreApplication::instance()->processEvents();
        return true;
      }
      catch(...)
      {
        qDebug() << "Error while replaying: " << cmd->key().toString().c_str()
                 << cmd->description();
        return false;
      }
    });

    return doc;
  }
  catch(std::runtime_error& e)
  {
    if(m_parentView)
      score::warning(m_parentView, QObject::tr("Error"), e.what());
    else
      qDebug() << "Error while loading: " << e.what();

    if(!doclist.empty() && doclist.back() == doc)
      doclist.pop_back();

    delete doc;
    return nullptr;
  }
}
}
