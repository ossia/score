#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <score_lib_process_export.h>

class QAction;

namespace score
{
class Document;
}

namespace Process
{

//! Adds the project file-management entries to the File menu, and tells the
//! user when a document it just loaded is missing some of its media.
class SCORE_LIB_PROCESS_EXPORT ProjectFilesApplicationPlugin final
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  explicit ProjectFilesApplicationPlugin(const score::GUIApplicationContext& ctx);
  ~ProjectFilesApplicationPlugin() override;

  score::GUIElements makeGUIElements() override;

  void on_loadedDocument(score::Document& doc) override;
  void on_documentSaveAs(score::Document& doc, const QString& newFileName) override;

private:
  void consolidate();
  void locateMissingFiles();
  void trimMedia();
  void archive();

  //! The document to act on, or nullptr; complains in the user's stead when
  //! the document has not been saved anywhere yet.
  score::Document* documentWithFolder(const QString& what);

  QAction* m_consolidate{};
  QAction* m_locate{};
  QAction* m_trim{};
  QAction* m_archive{};
};
}
