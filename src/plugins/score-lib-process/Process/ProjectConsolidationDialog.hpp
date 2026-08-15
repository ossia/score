#pragma once
#include <Process/FileReportView.hpp>
#include <Process/ProjectConsolidation.hpp>

#include <QDialog>

#include <score_lib_process_export.h>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

namespace score
{
struct DocumentContext;
}

namespace Process
{

/**
 * @brief Shows what consolidating the document would do, then does it.
 *
 * The plan is recomputed whenever an option changes, and it is the very same
 * traversal that runs on accept — what the user reads is what happens.
 */
class SCORE_LIB_PROCESS_EXPORT ProjectConsolidationDialog final : public QDialog
{
public:
  ProjectConsolidationDialog(const score::DocumentContext& ctx, QWidget* parent);
  ~ProjectConsolidationDialog();

  //! Filled once the dialog has been accepted.
  const FileReport& result() const noexcept { return m_result; }

private:
  void reanalyze();
  void run();
  void fill(const FileReport& report);
  score::ConsolidateOptions options() const noexcept;

  const score::DocumentContext& m_ctx;

  QLabel* m_summary{};
  QComboBox* m_mode{};
  QCheckBox* m_library{};
  QCheckBox* m_subfolders{};
  QCheckBox* m_keepFolderName{};
  FileReportView* m_files{};
  QPushButton* m_ok{};

  FileReport m_result;
};
}
