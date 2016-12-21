#pragma once
#include <QObject>
#include <QString>
#include <algorithm>
#include <core/document/Document.hpp>
#include <core/document/DocumentBuilder.hpp>
#include <iscore/tools/Version.hpp>
#include <iscore_lib_base_export.h>
#include <set>
#include <vector>
class QRecentFilesMenu;
namespace iscore
{
class Document;
struct ApplicationContext;
class View;
} // namespace iscore

namespace iscore
{
/**
 * @brief Owns the documents
 */
class ISCORE_LIB_BASE_EXPORT DocumentManager : public QObject
{
  Q_OBJECT
public:
  DocumentManager(iscore::View& view, QObject* parentPresenter);

  void init(const iscore::GUIApplicationContext& ctx);

  ~DocumentManager();

  auto recentFiles() const
  {
    return m_recentFiles;
  }

  // Document management
  Document*
  setupDocument(const iscore::GUIApplicationContext& ctx, iscore::Document* doc);

  template <typename... Args>
  void newDocument(const iscore::GUIApplicationContext& ctx, Args&&... args)
  {
    prepareNewDocument(ctx);
    setupDocument(
        ctx, m_builder.newDocument(ctx, std::forward<Args>(args)...));
  }

  template <typename... Args>
  Document* loadDocument(const iscore::GUIApplicationContext& ctx, Args&&... args)
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
  void restoreDocument(const iscore::GUIApplicationContext& ctx, Args&&... args)
  {
    prepareNewDocument(ctx);
    setupDocument(
        ctx, m_builder.restoreDocument(ctx, std::forward<Args>(args)...));
  }

  // Restore documents after a crash
  void restoreDocuments(const iscore::GUIApplicationContext& ctx);

  const std::vector<Document*>& documents() const
  {
    return m_documents;
  }
  std::vector<Document*>& documents()
  {
    return m_documents;
  }

  Document* currentDocument() const;
  void
  setCurrentDocument(const iscore::GUIApplicationContext& ctx, Document* doc);

  // Returns true if the document was closed.
  bool closeDocument(const iscore::GUIApplicationContext& ctx, Document&);
  void forceCloseDocument(const iscore::GUIApplicationContext& ctx, Document&);

  // Methods to save and load
  bool saveDocument(Document&);
  bool saveDocumentAs(Document&);

  bool saveStack();
  Document* loadStack(const iscore::GUIApplicationContext& ctx);
  Document* loadStack(const iscore::GUIApplicationContext& ctx, const QString&);

  Document* loadFile(const iscore::GUIApplicationContext& ctx);
  Document*
  loadFile(const iscore::GUIApplicationContext& ctx, const QString& filename);

  bool closeAllDocuments(const iscore::GUIApplicationContext& ctx);

  bool preparingNewDocument() const;

signals:
  void documentChanged(iscore::Document*);

private:
  void prepareNewDocument(const iscore::GUIApplicationContext& ctx);

  /**
   * @brief checkAndUpdateJson
   * @return boolean indicating if the document is loadable
   */
  bool
  checkAndUpdateJson(QJsonDocument&, const iscore::GUIApplicationContext& ctx);

  bool updateJson(
      QJsonObject& object,
      iscore::Version json_ver,
      iscore::Version iscore_ver);

  void saveRecentFilesState();

  iscore::View& m_view;

  DocumentBuilder m_builder;

  std::vector<Document*> m_documents;
  Document* m_currentDocument{};
  QPointer<QRecentFilesMenu> m_recentFiles{};

  bool m_preparingNewDocument{};
};

Id<iscore::DocumentModel> getStrongId(const std::vector<iscore::Document*>& v);
}
