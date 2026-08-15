#include <Process/MissingFilesDialog.hpp>

#include <score/document/DocumentContext.hpp>

#include <QApplication>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace Process
{
namespace
{
enum Column
{
  Owner = 0,
  File,
  Found
};

constexpr auto last_folder_setting = "Project/LastRelinkFolder";
}

MissingFilesDialog::MissingFilesDialog(
    const score::DocumentContext& ctx, QWidget* parent)
    : QDialog{parent}
    , m_ctx{ctx}
{
  setWindowTitle(tr("Missing files"));
  setAttribute(Qt::WA_DeleteOnClose);
  resize(860, 460);

  auto lay = new QVBoxLayout{this};

  m_summary = new QLabel{this};
  m_summary->setWordWrap(true);
  lay->addWidget(m_summary);

  m_files = new QTreeWidget{this};
  m_files->setRootIsDecorated(false);
  m_files->setAlternatingRowColors(true);
  m_files->setColumnCount(3);
  m_files->setHeaderLabels({tr("Used by"), tr("Missing file"), tr("Found at")});
  m_files->header()->setSectionResizeMode(Column::File, QHeaderView::Stretch);
  m_files->header()->setSectionResizeMode(Column::Found, QHeaderView::Stretch);
  lay->addWidget(m_files, 1);

  auto tools = new QHBoxLayout;
  lay->addLayout(tools);

  m_search = new QPushButton{tr("Search a folder..."), this};
  m_search->setToolTip(
      tr("Looks through that folder and everything under it for files with the "
         "same names. Nothing is changed until you apply."));
  tools->addWidget(m_search);

  m_locate = new QPushButton{tr("Locate..."), this};
  m_locate->setToolTip(tr("Pick the file for the selected row by hand."));
  tools->addWidget(m_locate);
  tools->addStretch(1);

  auto buttons
      = new QDialogButtonBox{QDialogButtonBox::Apply | QDialogButtonBox::Close, this};
  m_apply = buttons->button(QDialogButtonBox::Apply);
  m_apply->setText(tr("Relink"));
  lay->addWidget(buttons);

  connect(m_search, &QPushButton::clicked, this, &MissingFilesDialog::searchFolder);
  connect(m_locate, &QPushButton::clicked, this, &MissingFilesDialog::locateSelected);
  connect(m_apply, &QPushButton::clicked, this, &MissingFilesDialog::applyRelink);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
  connect(
      m_files, &QTreeWidget::itemSelectionChanged, this,
      [this] { m_locate->setEnabled(m_files->currentItem() != nullptr); });

  m_lastSearchFolder = QSettings{}.value(last_folder_setting).toString();

  rescan();
}

MissingFilesDialog::~MissingFilesDialog() = default;

bool MissingFilesDialog::nothingMissing(const score::DocumentContext& ctx)
{
  return scanMissingFiles(ctx).count(FileAction::Missing) == 0;
}

void MissingFilesDialog::rescan()
{
  m_report = scanMissingFiles(m_ctx);
  m_files->clear();

  QList<QTreeWidgetItem*> items;
  for(const auto* e : m_report.with(FileAction::Missing))
  {
    auto* item = new QTreeWidgetItem{{e->owner, e->storedPath, tr("not found")}};
    item->setData(Column::File, Qt::UserRole, e->storedPath);
    items.push_back(item);
  }
  m_files->addTopLevelItems(items);

  // A reference the user already resolved keeps its answer across a rescan.
  for(auto it = m_resolutions.constBegin(); it != m_resolutions.constEnd(); ++it)
  {
    if(it->chosen < 0)
      continue;
    if(auto* item = itemFor(it.key()))
      item->setText(Column::Found, it->candidates[it->chosen]);
  }

  updateSummary();
}

QTreeWidgetItem* MissingFilesDialog::itemFor(const QString& storedPath) const
{
  for(int i = 0; i < m_files->topLevelItemCount(); i++)
  {
    auto* item = m_files->topLevelItem(i);
    if(item->data(Column::File, Qt::UserRole).toString() == storedPath)
      return item;
  }
  return nullptr;
}

void MissingFilesDialog::updateSummary()
{
  const int missing = m_files->topLevelItemCount();
  int resolved = 0;
  for(auto it = m_resolutions.constBegin(); it != m_resolutions.constEnd(); ++it)
    if(it->chosen >= 0 && itemFor(it.key()))
      ++resolved;

  if(missing == 0)
  {
    m_summary->setText(tr("Every file this project uses was found."));
  }
  else
  {
    m_summary->setText(
        tr("<b>%1 file(s) cannot be found.</b> The project still opens and plays "
           "everything else; the processes using them stay silent until they are "
           "relinked.<br/>%2 of them have a candidate ready to apply.")
            .arg(missing)
            .arg(resolved));
  }

  m_apply->setEnabled(resolved > 0);
  m_locate->setEnabled(m_files->currentItem() != nullptr);
}

void MissingFilesDialog::searchFolder()
{
  const QString folder = QFileDialog::getExistingDirectory(
      this, tr("Look for the missing files in"), m_lastSearchFolder);
  if(folder.isEmpty())
    return;

  m_lastSearchFolder = folder;
  QSettings{}.setValue(last_folder_setting, folder);

  FileIndex index;
  {
    // Indexing a large drive takes a while and gives no feedback otherwise.
    QApplication::setOverrideCursor(Qt::WaitCursor);
    index.scan(folder);
    QApplication::restoreOverrideCursor();
  }

  int found = 0;
  for(int i = 0; i < m_files->topLevelItemCount(); i++)
  {
    auto* item = m_files->topLevelItem(i);
    const QString stored = item->data(Column::File, Qt::UserRole).toString();

    auto candidates = index.candidates(stored);
    if(candidates.empty())
      continue;

    Resolution res{std::move(candidates), 0};
    item->setText(
        Column::Found,
        res.candidates.size() == 1
            ? res.candidates[0]
            : tr("%1  (+%2 other candidate(s))")
                  .arg(res.candidates[0])
                  .arg(res.candidates.size() - 1));
    item->setToolTip(Column::Found, res.candidates[0]);
    m_resolutions.insert(stored, std::move(res));
    ++found;
  }

  if(found == 0)
  {
    QMessageBox::information(
        this, tr("Nothing found"),
        index.truncated()
            ? tr("No matching file name under %1 -- and the search stopped early "
                 "because that folder holds too many files. Try pointing it at a "
                 "narrower folder.")
                  .arg(folder)
            : tr("No file with a matching name was found under %1.").arg(folder));
  }

  updateSummary();
}

void MissingFilesDialog::locateSelected()
{
  auto* item = m_files->currentItem();
  if(!item)
    return;

  const QString stored = item->data(Column::File, Qt::UserRole).toString();
  const QString name = QFileInfo{stored}.fileName();

  const QString picked = QFileDialog::getOpenFileName(
      this, tr("Locate %1").arg(name), m_lastSearchFolder);
  if(picked.isEmpty())
    return;

  m_lastSearchFolder = QFileInfo{picked}.absolutePath();
  m_resolutions.insert(stored, Resolution{{picked}, 0});
  item->setText(Column::Found, picked);
  item->setToolTip(Column::Found, picked);

  updateSummary();
}

void MissingFilesDialog::applyRelink()
{
  QHash<QString, QString> chosen;
  for(auto it = m_resolutions.constBegin(); it != m_resolutions.constEnd(); ++it)
    if(it->chosen >= 0 && itemFor(it.key()))
      chosen.insert(it.key(), it->candidates[it->chosen]);

  if(chosen.isEmpty())
    return;

  const auto report = relinkFiles(m_ctx, chosen);

  if(const int failed = report.count(FileAction::Failed); failed > 0)
  {
    QString detail;
    for(const auto* e : report.with(FileAction::Failed))
      detail += e->note + '\n';
    QMessageBox::warning(this, tr("Some files could not be relinked"), detail);
  }

  m_resolutions.clear();
  rescan();
}
}
