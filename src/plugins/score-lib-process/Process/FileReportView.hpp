#pragma once
#include <Process/FileOperation.hpp>

#include <QTreeWidget>

#include <score_lib_process_export.h>

namespace Process
{

/**
 * @brief The table every file operation shows its report in.
 *
 * One widget rather than one per dialog: consolidating, relinking and trimming
 * all answer the same questions -- which file, used by what, what happens to
 * it, how big -- and a user who has read one of these lists has read them all.
 */
class SCORE_LIB_PROCESS_EXPORT FileReportView final : public QTreeWidget
{
public:
  explicit FileReportView(QWidget* parent = nullptr);
  ~FileReportView();

  //! Replace the contents. Entries whose action is in `hidden` are left out.
  void setReport(const FileReport& report, const std::vector<FileAction>& hidden = {});

  //! Show a checkbox on every row, so the user picks what to apply.
  void setCheckable(bool);

  //! Stored paths of the checked rows.
  std::vector<QString> checkedPaths() const;

private:
  bool m_checkable{};
};
}
