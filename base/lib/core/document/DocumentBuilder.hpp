#pragma once
class QByteArray;
class QVariant;
#include <iscore/model/Identifier.hpp>
#include <iscore_lib_base_export.h>
namespace iscore
{
class Document;
class DocumentBackupManager;
class DocumentDelegateFactory;
class DocumentModel;
struct GUIApplicationContext;

/**
 * @brief Methods to set-up documents.
 *
 * Facility to construct a Document according to different cases :
 * - Creating a blank, new document.
 * - Loading a document.
 * - Restoring a document after a crash.
 *
 */
class ISCORE_LIB_BASE_EXPORT DocumentBuilder
{
public:
  explicit DocumentBuilder(QObject* parentPresenter, QWidget* parentView);

  Document* newDocument(
      const iscore::GUIApplicationContext& ctx,
      const Id<DocumentModel>& id,
      iscore::DocumentDelegateFactory& doctype);
  Document* loadDocument(
      const iscore::GUIApplicationContext& ctx,
      QString filename,
      const QVariant& data,
      iscore::DocumentDelegateFactory& doctype);
  Document* restoreDocument(
      const iscore::GUIApplicationContext& ctx,
      QString filename,
      const QByteArray& docData,
      const QByteArray& cmdData,
      iscore::DocumentDelegateFactory& doctype);

private:
  void setBackupManager(Document* doc);

  QObject* m_parentPresenter{};
  QWidget* m_parentView{};

  DocumentBackupManager* m_backupManager{};
};
}
