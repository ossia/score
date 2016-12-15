#include <QByteArray>
#include <QMessageBox>
#include <QObject>
#include <core/document/Document.hpp>
#include <core/document/DocumentBackupManager.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/View.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>

#include <QString>
#include <QVariant>
#include <stdexcept>

#include "DocumentBuilder.hpp"
#include <core/command/CommandStackSerialization.hpp>
#include <iscore/application/ApplicationComponents.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>

#include <iscore/tools/RandomNameProvider.hpp>
#include <iscore/model/Identifier.hpp>

namespace iscore
{
DocumentBuilder::DocumentBuilder(QObject* parentPresenter, QWidget* parentView)
    : m_parentPresenter{parentPresenter}, m_parentView{parentView}
{
}

ISCORE_LIB_BASE_EXPORT
Document* DocumentBuilder::newDocument(
    const iscore::GUIApplicationContext& ctx,
    const Id<DocumentModel>& id,
    DocumentDelegateFactory& doctype)
{
  QString docName = "Untitled." + RandomNameProvider::generateRandomName();
  auto doc
      = new Document{docName, id, doctype, m_parentView, m_parentPresenter};

  m_backupManager = new DocumentBackupManager{*doc};
  for (auto& appPlug : ctx.applicationPlugins())
  {
    appPlug->on_newDocument(*doc);
  }

  for (auto& appPlug : ctx.applicationPlugins())
  {
    appPlug->on_createdDocument(*doc);
  }
  // First save
  m_backupManager->saveModelData(doc->saveAsByteArray());
  setBackupManager(doc);

  return doc;
}

ISCORE_LIB_BASE_EXPORT
Document* DocumentBuilder::loadDocument(
    const iscore::GUIApplicationContext& ctx,
    const QVariant& docData,
    DocumentDelegateFactory& doctype)
{
  Document* doc = nullptr;
  try
  {
    doc = new Document{docData, doctype, m_parentView, m_parentPresenter};
    for (auto& appPlug : ctx.applicationPlugins())
    {
      appPlug->on_loadedDocument(*doc);
    }

    for (auto& appPlug : ctx.applicationPlugins())
    {
      appPlug->on_createdDocument(*doc);
    }

    ctx.documents.documents().push_back(doc);

    m_backupManager = new DocumentBackupManager{*doc};
    m_backupManager->saveModelData(doc->saveAsByteArray());
    setBackupManager(doc);

    return doc;
  }
  catch (std::runtime_error& e)
  {
    QMessageBox::warning(m_parentView, QObject::tr("Error"), e.what());

    if (!ctx.documents.documents().empty()
        && ctx.documents.documents().back() == doc)
      ctx.documents.documents().pop_back();

    delete doc;
    return nullptr;
  }
}
ISCORE_LIB_BASE_EXPORT
Document* DocumentBuilder::restoreDocument(
    const iscore::GUIApplicationContext& ctx,
    const QByteArray& docData,
    const QByteArray& cmdData,
    DocumentDelegateFactory& doctype)
{

  Document* doc = nullptr;
  try
  {
    // Restoring behaves just like loading : we reload what was loaded
    // (potentially a blank document which is saved at the beginning, once
    // every plug-in has been loaded)
    doc = new Document{docData, doctype, m_parentView, m_parentPresenter};
    for (auto& appPlug : ctx.applicationPlugins())
    {
      appPlug->on_loadedDocument(*doc);
    }

    for (auto& appPlug : ctx.applicationPlugins())
    {
      appPlug->on_createdDocument(*doc);
    }

    ctx.documents.documents().push_back(doc);

    // We restore the pre-crash command stack.
    Deserializer<DataStream> writer(cmdData);
    loadCommandStack(
        ctx.components, writer, doc->commandStack(), [](auto cmd) {
          cmd->redo();
        });

    m_backupManager = new DocumentBackupManager{*doc};
    m_backupManager->saveModelData(docData); // Reuse the same data
    setBackupManager(doc);

    return doc;
  }
  catch (std::runtime_error& e)
  {
    QMessageBox::warning(m_parentView, QObject::tr("Error"), e.what());

    if (!ctx.documents.documents().empty()
        && ctx.documents.documents().back() == doc)
      ctx.documents.documents().pop_back();

    delete doc;
    return nullptr;
  }
}

void DocumentBuilder::setBackupManager(Document* doc)
{
  m_backupManager->updateBackupData();
  doc->setBackupMgr(m_backupManager);
  m_backupManager = nullptr;
}
}
