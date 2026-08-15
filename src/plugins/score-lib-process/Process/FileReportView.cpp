#include <Process/FileReportView.hpp>

#include <QHeaderView>
#include <QLocale>

namespace Process
{
namespace
{
enum Column
{
  Owner = 0,
  File,
  Type,
  Size,
  Becomes
};

QString sizeText(const FileEntry& e)
{
  const QLocale loc;
  if(e.action == FileAction::Trimmed && e.newSize > 0)
    return QStringLiteral("%1 → %2")
        .arg(loc.formattedDataSize(e.size), loc.formattedDataSize(e.newSize));
  if(e.size > 0)
    return loc.formattedDataSize(e.size);
  return QStringLiteral("-");
}

//! What the reference becomes, or why it does not change.
QString outcomeText(const FileEntry& e)
{
  if(!e.note.isEmpty())
    return e.note;
  if(!e.newStoredPath.isEmpty() && e.newStoredPath != e.storedPath)
    return e.newStoredPath;
  return toString(e.action);
}
}

FileReportView::FileReportView(QWidget* parent)
    : QTreeWidget{parent}
{
  setRootIsDecorated(false);
  setAlternatingRowColors(true);
  setUniformRowHeights(true);
  setColumnCount(5);
  setHeaderLabels(
      {tr("Used by"), tr("File"), tr("Action"), tr("Size"), tr("Becomes")});
  header()->setSectionResizeMode(Column::File, QHeaderView::Stretch);
  header()->setSectionResizeMode(Column::Becomes, QHeaderView::Stretch);
}

FileReportView::~FileReportView() = default;

void FileReportView::setCheckable(bool b)
{
  m_checkable = b;
}

void FileReportView::setReport(
    const FileReport& report, const std::vector<FileAction>& hidden)
{
  clear();

  const auto isHidden = [&](FileAction a) {
    return std::find(hidden.begin(), hidden.end(), a) != hidden.end();
  };

  QList<QTreeWidgetItem*> items;
  items.reserve(report.entries.size());
  for(const auto& e : report.entries)
  {
    if(isHidden(e.action))
      continue;

    auto* item = new QTreeWidgetItem{
        {e.owner, e.storedPath, toString(e.action), sizeText(e), outcomeText(e)}};

    item->setToolTip(Column::File, e.sourcePath);
    if(!e.destinationPath.isEmpty())
      item->setToolTip(Column::Becomes, e.destinationPath);
    if(!e.note.isEmpty())
      item->setToolTip(Column::Type, e.note);

    // The stored path is the identity of a reference: it is what the caller
    // matches against when applying only the checked rows.
    item->setData(Column::File, Qt::UserRole, e.storedPath);

    if(m_checkable)
    {
      item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
      item->setCheckState(
          Column::Owner,
          e.action == FileAction::Missing ? Qt::Unchecked : Qt::Checked);
    }

    items.push_back(item);
  }
  addTopLevelItems(items);
}

std::vector<QString> FileReportView::checkedPaths() const
{
  std::vector<QString> out;
  for(int i = 0; i < topLevelItemCount(); i++)
  {
    auto* item = topLevelItem(i);
    if(item->checkState(Column::Owner) == Qt::Checked)
      out.push_back(item->data(Column::File, Qt::UserRole).toString());
  }
  return out;
}
}
