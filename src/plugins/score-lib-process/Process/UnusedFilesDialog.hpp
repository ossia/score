#pragma once
#include <Process/FileReportView.hpp>
#include <Process/UnusedFiles.hpp>

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
 * @brief Lists what is in the project folder and no longer used, and removes
 *        the ones the user ticks.
 *
 * The list is checkable rather than a summary: this deletes files, so the set
 * that goes is the set the user looked at and agreed to, one row at a time if
 * they want. Whatever reason score has to be less than certain about a file
 * being unused is spelled out above the list rather than buried in a manual.
 */
class SCORE_LIB_PROCESS_EXPORT UnusedFilesDialog final : public QDialog
{
public:
  UnusedFilesDialog(const score::DocumentContext& ctx, QWidget* parent);
  ~UnusedFilesDialog();

  const FileReport& result() const noexcept { return m_result; }

private:
  void rescan();
  void run();
  UnusedFilesOptions options() const noexcept;

  const score::DocumentContext& m_ctx;

  QLabel* m_summary{};
  QLabel* m_warnings{};
  QCheckBox* m_everywhere{};
  QComboBox* m_disposal{};
  FileReportView* m_files{};
  QPushButton* m_ok{};

  FileReport m_scan;
  FileReport m_result;
};
}
