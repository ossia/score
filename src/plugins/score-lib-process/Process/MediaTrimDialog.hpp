#pragma once
#include <Process/FileReportView.hpp>
#include <Process/MediaTrim.hpp>

#include <QDialog>

#include <score_lib_process_export.h>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace score
{
struct DocumentContext;
}

namespace Process
{

/**
 * @brief Shows what trimming would remove, and asks before removing it.
 *
 * This is the one operation in score's file handling that can lose audio, so
 * the dialog is built around the plan rather than around the options: every
 * file that would change is listed with its before and after size, and every
 * file that would not is listed with the reason, because "it silently did
 * nothing" is the complaint every other implementation collects.
 */
class SCORE_LIB_PROCESS_EXPORT MediaTrimDialog final : public QDialog
{
public:
  MediaTrimDialog(const score::DocumentContext& ctx, QWidget* parent);
  ~MediaTrimDialog();

  const FileReport& result() const noexcept { return m_result; }

private:
  void reanalyze();
  void run();
  void fill(const FileReport& report);
  TrimOptions options() const noexcept;

  const score::DocumentContext& m_ctx;

  QLabel* m_summary{};
  QDoubleSpinBox* m_handles{};
  QCheckBox* m_removeOriginal{};
  FileReportView* m_files{};
  QPushButton* m_ok{};

  FileReport m_result;
};
}
