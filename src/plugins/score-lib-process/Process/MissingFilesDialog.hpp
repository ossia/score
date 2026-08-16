#pragma once
#include <Process/MissingFiles.hpp>

#include <QDialog>
#include <QHash>
#include <QPointer>

#include <score_lib_process_export.h>

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace score
{
class Document;
struct DocumentContext;
}

namespace Process
{

/**
 * @brief Lists what a document cannot find, and helps point it at the files.
 *
 * Deliberately not modal-on-load: an alarming box in front of a project the
 * user just opened is the single most complained-about part of this feature
 * everywhere it exists. It opens beside the document, says what is missing,
 * and does nothing until asked.
 *
 * Nothing is ever relinked automatically either. Searching a folder fills in
 * the candidates it found -- best match first, exact size preferred over a
 * mere name match -- and the user applies them.
 */
class SCORE_LIB_PROCESS_EXPORT MissingFilesDialog final : public QDialog
{
public:
  MissingFilesDialog(score::Document& doc, QWidget* parent);
  ~MissingFilesDialog();

  //! True when the document has nothing missing, so the caller can skip it.
  static bool nothingMissing(const score::DocumentContext& ctx);

private:
  void rescan();
  void searchFolder();
  void locateSelected();
  void applyRelink();
  void updateSummary();

  //! Candidates found for one missing reference, best first.
  struct Resolution
  {
    std::vector<QString> candidates;
    int chosen{-1};
  };

  QTreeWidgetItem* itemFor(const QString& storedPath) const;

  /** The document, not its context.
   *
   * This dialog is deliberately not modal -- it opens beside a document that
   * has just been loaded -- so the document can be closed while it is still
   * on screen. A DocumentContext lives inside its Document and dies with it;
   * a guarded pointer lets the dialog notice and close itself instead of
   * relinking through freed memory.
   */
  QPointer<score::Document> m_doc;

  QLabel* m_summary{};
  QTreeWidget* m_files{};
  QPushButton* m_search{};
  QPushButton* m_locate{};
  QPushButton* m_apply{};

  FileReport m_report;
  //! stored path -> what we would relink it to
  QHash<QString, Resolution> m_resolutions;
  QString m_lastSearchFolder;
};
}
