#pragma once
class QByteArray;
class QVariant;
#include <score/model/Identifier.hpp>
#include <score_lib_base_export.h>
namespace score
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
class SCORE_LIB_BASE_EXPORT DocumentBuilder
{
public:
  explicit DocumentBuilder(QObject* parentPresenter, QWidget* parentView);

  Document* newDocument(
      const score::GUIApplicationContext& ctx,
      const Id<DocumentModel>& id,
      score::DocumentDelegateFactory& doctype);
  Document* loadDocument(
      const score::GUIApplicationContext& ctx,
      QString filename,
      const QVariant& data,
      score::DocumentDelegateFactory& doctype);
  Document* restoreDocument(
      const score::GUIApplicationContext& ctx,
      QString filename,
      const QByteArray& docData,
      const QByteArray& cmdData,
      score::DocumentDelegateFactory& doctype);

private:
  void setBackupManager(Document* doc);

  QObject* m_parentPresenter{};
  QWidget* m_parentView{};

  DocumentBackupManager* m_backupManager{};
};
}
