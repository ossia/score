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
#include <core/document/DocumentTemplates.hpp>
#include <core/document/ProjectInfo.hpp>
#include <score/plugins/documentdelegate/plugin/SerializableDocumentPlugin.hpp>
#include <ossia/detail/algorithms.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/Window.hpp>

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QObject>
#include <QString>

#include <stdexcept>

namespace score
{
DocumentBuilder::DocumentBuilder(QObject* parentPresenter, QWidget* parentView)
    : m_parentPresenter{parentPresenter}
    , m_parentView{parentView}
{
}

namespace
{
// A document saved before a project settings plug-in existed has no model for
// it: create the missing ones so that ctx.plugin<T>() always works.
void ensureProjectSettingsModels(const score::GUIApplicationContext& ctx, Document& doc)
{
  auto& model = doc.model();
  for(auto& fact : ctx.interfaces<DocumentPluginFactoryList>())
  {
    auto settings = dynamic_cast<ProjectSettingsFactory*>(&fact);
    if(!settings)
      continue;

    const auto key = fact.concreteKey();
    const bool present = ossia::any_of(model.pluginModels(), [&](DocumentPlugin* p) {
      auto sp = dynamic_cast<SerializableDocumentPlugin*>(p);
      return sp && sp->concreteKey() == key;
    });
    if(!present)
      model.addPluginModel(settings->makeModel(doc.context(), &model));
  }
}
}

SCORE_LIB_BASE_EXPORT
Document* DocumentBuilder::newDocument(
    const score::GUIApplicationContext& ctx, const Id<DocumentModel>& id,
    DocumentDelegateFactory& doctype)
{
  if(const auto tpl = defaultDocumentTemplate(); !tpl.isEmpty())
  {
    if(auto doc = newDocumentFromTemplate(ctx, id, tpl, doctype))
      return doc;
  }

  QString docName = "Untitled." + RandomNameProvider::generateShortRandomName();
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
Document* DocumentBuilder::newDocumentFromTemplate(
    const score::GUIApplicationContext& ctx, const Id<DocumentModel>& id,
    const QString& templatePath, DocumentDelegateFactory& doctype)
{
  if(!QFile::exists(templatePath))
    return nullptr;

  try
  {
    auto doc = loadDocument(ctx, templatePath, doctype);
    if(!doc)
      return nullptr;

    // Detach from the template file
    doc->metadata().setFileName(
        "Untitled." + RandomNameProvider::generateShortRandomName());
    doc->model().setId(id);
    // TODO cables ?!

    if(auto info = doc->context().findPlugin<ProjectInfo::Model>())
    {
      info->setCreated(QDateTime::currentDateTime());
      info->setLastSaved({});
    }
    return doc;
  }
  catch(...)
  {
    return nullptr;
  }
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
    ensureProjectSettingsModels(ctx, *doc);
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
    ensureProjectSettingsModels(ctx, *doc);
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
    ensureProjectSettingsModels(ctx, *doc);
    for(auto& appPlug : ctx.guiApplicationPlugins())
    {
      appPlug->on_loadedDocument(*doc);
    }

    for(auto& appPlug : ctx.guiApplicationPlugins())
    {
      appPlug->on_createdDocument(*doc);
    }

    doclist.push_back(doc);

    QMetaObject::invokeMethod(doc, [doc, restore] {
      // We restore the pre-crash command stack.
      doc->m_loaded = false;
      DataStream::Deserializer writer(restore.commands);
      loadCommandStack(
          doc->context().app.components, writer, doc->commandStack(),
          [doc](score::Command* cmd) {
        try
        {
          cmd->redo(doc->context());
          return true;
        }
        catch(...)
        {
          qDebug() << "Error while replaying: " << cmd->key().toString().c_str()
                   << cmd->description();
          return false;
        }
      });
      doc->m_loaded = true;
    }, Qt::QueuedConnection);

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
