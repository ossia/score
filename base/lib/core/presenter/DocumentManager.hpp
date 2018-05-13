#pragma once
#include <QObject>
#include <wobjectdefs.h>
#include <QString>
#include <algorithm>
#include <core/document/Document.hpp>
#include <core/document/DocumentBuilder.hpp>
#include <score/tools/Version.hpp>
#include <score_lib_base_export.h>
#include <set>
#include <vector>
class QRecentFilesMenu;
namespace score
{
class Document;
struct ApplicationContext;
class View;
} // namespace score

namespace score
{

struct DocumentList
{
public:
  const std::vector<Document*>& documents() const
  {
    return m_documents;
  }

  std::vector<Document*>& documents()
  {
    return m_documents;
  }

  Document* currentDocument() const
  {
    return m_currentDocument;
  }

  void setCurrentDocument(Document* d)
  {
    m_currentDocument = d;
  }

protected:
  std::vector<Document*> m_documents;
  Document* m_currentDocument{};
};

/**
 * @brief Owns the documents
 */
class SCORE_LIB_BASE_EXPORT DocumentManager
    : public QObject
    , public DocumentList
{
  W_OBJECT(DocumentManager)
public:
  DocumentManager(score::View* view, QObject* parentPresenter);

  void init(const score::GUIApplicationContext& ctx);

  ~DocumentManager();

  auto recentFiles() const
  {
    return m_recentFiles;
  }

  // Document management
  Document*
  setupDocument(const score::GUIApplicationContext& ctx, score::Document* doc);

  template <typename... Args>
  void newDocument(const score::GUIApplicationContext& ctx, Args&&... args)
  {
    prepareNewDocument(ctx);
    setupDocument(
        ctx, m_builder.newDocument(ctx, std::forward<Args>(args)...));
  }

  template <typename... Args>
  Document*
  loadDocument(const score::GUIApplicationContext& ctx, Args&&... args)
  {
    auto cur = currentDocument();
    if (cur && cur->virgin())
    {
      forceCloseDocument(ctx, *cur);
    }
    prepareNewDocument(ctx);
    return setupDocument(
        ctx, m_builder.loadDocument(ctx, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void restoreDocument(const score::GUIApplicationContext& ctx, Args&&... args)
  {
    prepareNewDocument(ctx);
    setupDocument(
        ctx, m_builder.restoreDocument(ctx, std::forward<Args>(args)...));
  }

  // Restore documents after a crash
  void restoreDocuments(const score::GUIApplicationContext& ctx);

  void
  setCurrentDocument(const score::GUIApplicationContext& ctx, Document* doc);

  // Returns true if the document was closed.
  bool closeDocument(const score::GUIApplicationContext& ctx, Document&);
  void forceCloseDocument(const score::GUIApplicationContext& ctx, Document&);

  // Methods to save and load
  bool saveDocument(Document&);
  bool saveDocumentAs(Document&);

  bool saveStack();
  Document* loadStack(const score::GUIApplicationContext& ctx);
  Document* loadStack(const score::GUIApplicationContext& ctx, const QString&);

  Document* loadFile(const score::GUIApplicationContext& ctx);
  Document*
  loadFile(const score::GUIApplicationContext& ctx, const QString& filename);

  bool closeAllDocuments(const score::GUIApplicationContext& ctx);

  bool preparingNewDocument() const;

public:
  void documentChanged(score::Document* arg_1) W_SIGNAL(documentChanged, arg_1);

private:
  void prepareNewDocument(const score::GUIApplicationContext& ctx);

  /**
   * @brief checkAndUpdateJson
   * @return boolean indicating if the document is loadable
   */
  bool
  checkAndUpdateJson(QJsonDocument&, const score::GUIApplicationContext& ctx);

  bool updateJson(
      QJsonObject& object, score::Version json_ver, score::Version score_ver);

  void saveRecentFilesState();

  score::View* m_view{};

  DocumentBuilder m_builder;

  QPointer<QRecentFilesMenu> m_recentFiles{};

  bool m_preparingNewDocument{};
};

Id<score::DocumentModel> getStrongId(const std::vector<score::Document*>& v);
Id<score::DocumentPlugin>
getStrongId(const std::vector<score::DocumentPlugin*>& v);
}
