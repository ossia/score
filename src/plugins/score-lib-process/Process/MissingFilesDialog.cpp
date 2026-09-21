#include <Process/MissingFilesDialog.hpp>

#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace Process
{
namespace
{
//! See the note in FileReportView.cpp: anonymous namespaces are merged across
//! this library under a unity build.
enum class MissingColumn : int
{
  Owner = 0,
  File,
  Found
};

constexpr auto last_folder_setting = "Project/LastRelinkFolder";
constexpr auto header_setting = "Project/MissingFilesHeader";

//! Size of the missing file, when the document knows it, for ranking.
constexpr int SizeRole = Qt::UserRole + 1;
}

MissingFilesDialog::MissingFilesDialog(score::Document& doc, QWidget* parent)
    : QDialog{parent}
    , m_doc{&doc}
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
  lay->addWidget(m_files, 1);

  // Stretched sections cannot be dragged.
  auto* header = m_files->header();
  header->setSectionResizeMode(QHeaderView::Interactive);
  header->setSectionsMovable(true);
  header->setStretchLastSection(false);
  header->resizeSection((int)MissingColumn::Owner, 180);
  header->resizeSection((int)MissingColumn::File, 320);
  header->resizeSection((int)MissingColumn::Found, 320);
  header->restoreState(QSettings{}.value(header_setting).toByteArray());

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

  m_progressRow = new QWidget{this};
  {
    auto progressLayout = new QHBoxLayout{m_progressRow};
    progressLayout->setContentsMargins(0, 0, 0, 0);

    m_progress = new QLabel{m_progressRow};
    m_progress->setTextFormat(Qt::PlainText);
    m_progress->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    progressLayout->addWidget(m_progress, 1);

    m_cancel = new QPushButton{tr("Cancel"), m_progressRow};
    progressLayout->addWidget(m_cancel);
  }
  m_progressRow->hide();
  lay->addWidget(m_progressRow);

  m_progressTimer = new QTimer{this};
  m_progressTimer->setInterval(100);

  auto buttons
      = new QDialogButtonBox{QDialogButtonBox::Apply | QDialogButtonBox::Close, this};
  m_apply = buttons->button(QDialogButtonBox::Apply);
  m_apply->setText(tr("Relink"));
  lay->addWidget(buttons);

  connect(m_search, &QPushButton::clicked, this, &MissingFilesDialog::searchFolder);
  connect(m_cancel, &QPushButton::clicked, this, &MissingFilesDialog::cancelSearch);
  connect(
      m_progressTimer, &QTimer::timeout, this,
      &MissingFilesDialog::updateSearchProgress);
  connect(m_locate, &QPushButton::clicked, this, &MissingFilesDialog::locateSelected);
  connect(m_apply, &QPushButton::clicked, this, &MissingFilesDialog::applyRelink);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
  connect(
      m_files, &QTreeWidget::itemSelectionChanged, this,
      [this] { m_locate->setEnabled(m_files->currentItem() != nullptr); });

  m_lastSearchFolder = QSettings{}.value(last_folder_setting).toString();

  rescan();
}

MissingFilesDialog::~MissingFilesDialog()
{
  if(m_scan)
    m_scan->cancel();

  QSettings{}.setValue(header_setting, m_files->header()->saveState());
}

bool MissingFilesDialog::nothingMissing(const score::DocumentContext& ctx)
{
  return scanMissingFiles(ctx).count(FileAction::Missing) == 0;
}

void MissingFilesDialog::rescan()
{
  // The document can be closed while this window is up: it is not modal on
  // purpose. Go away rather than read a context that no longer exists.
  if(!m_doc)
  {
    close();
    return;
  }

  m_report = scanMissingFiles(m_doc->context());
  m_files->clear();

  QList<QTreeWidgetItem*> items;
  for(const auto* e : m_report.with(FileAction::Missing))
  {
    auto* item = new QTreeWidgetItem{{e->owner, e->storedPath, tr("not found")}};
    item->setData((int)MissingColumn::File, Qt::UserRole, e->storedPath);
    item->setData((int)MissingColumn::File, SizeRole, e->size);
    items.push_back(item);
  }
  m_files->addTopLevelItems(items);

  // A reference the user already resolved keeps its answer across a rescan.
  for(auto it = m_resolutions.constBegin(); it != m_resolutions.constEnd(); ++it)
  {
    if(it->chosen < 0)
      continue;
    if(auto* item = itemFor(it.key()))
      item->setText((int)MissingColumn::Found, it->candidates[it->chosen]);
  }

  updateSummary();
}

QTreeWidgetItem* MissingFilesDialog::itemFor(const QString& storedPath) const
{
  for(int i = 0; i < m_files->topLevelItemCount(); i++)
  {
    auto* item = m_files->topLevelItem(i);
    if(item->data((int)MissingColumn::File, Qt::UserRole).toString() == storedPath)
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
  if(m_scan)
    return;

  const QString folder = QFileDialog::getExistingDirectory(
      this, tr("Look for the missing files in"), m_lastSearchFolder);
  if(folder.isEmpty())
    return;

  m_lastSearchFolder = folder;
  m_searchRoot = folder;
  QSettings{}.setValue(last_folder_setting, folder);

  m_search->setEnabled(false);
  m_cancel->setEnabled(true);
  m_progress->setText(tr("Searching %1...").arg(folder));
  m_progressRow->show();

  m_scan = FileScan::start(folder, this, [this](FileIndex index, bool cancelled) {
    finishSearch(std::move(index), cancelled);
  });
  m_progressTimer->start();
}

void MissingFilesDialog::cancelSearch()
{
  if(!m_scan)
    return;

  m_scan->cancel();
  m_cancel->setEnabled(false);
  m_progressTimer->stop();
  m_progress->setText(tr("Stopping..."));
}

void MissingFilesDialog::updateSearchProgress()
{
  if(!m_scan)
    return;

  const QString text = tr("%1 file(s) seen -- %2")
                           .arg(m_scan->filesSeen())
                           .arg(m_scan->currentFolder());

  const int room = m_progress->width();
  m_progress->setText(
      room > 0 ? m_progress->fontMetrics().elidedText(text, Qt::ElideMiddle, room)
               : text);
}

void MissingFilesDialog::finishSearch(FileIndex index, bool cancelled)
{
  m_progressTimer->stop();
  m_progressRow->hide();
  m_search->setEnabled(true);
  m_scan.reset();

  int found = 0;
  for(int i = 0; i < m_files->topLevelItemCount(); i++)
  {
    auto* item = m_files->topLevelItem(i);
    const QString stored = item->data((int)MissingColumn::File, Qt::UserRole).toString();
    const qint64 size = item->data((int)MissingColumn::File, SizeRole).toLongLong();

    auto candidates = index.candidates(stored, size);
    if(candidates.empty())
      continue;

    Resolution res{std::move(candidates), 0};
    item->setText(
        (int)MissingColumn::Found,
        res.candidates.size() == 1
            ? res.candidates[0]
            : tr("%1  (+%2 other candidate(s))")
                  .arg(res.candidates[0])
                  .arg(res.candidates.size() - 1));
    item->setToolTip((int)MissingColumn::Found, res.candidates[0]);
    m_resolutions.insert(stored, std::move(res));
    ++found;
  }

  if(found == 0 && !cancelled)
  {
    QMessageBox::information(
        this, tr("Nothing found"),
        tr("No file with a matching name was found under %1.").arg(m_searchRoot));
  }

  updateSummary();
}

void MissingFilesDialog::locateSelected()
{
  auto* item = m_files->currentItem();
  if(!item)
    return;

  const QString stored = item->data((int)MissingColumn::File, Qt::UserRole).toString();
  const QString name = QFileInfo{stored}.fileName();

  const QString picked = QFileDialog::getOpenFileName(
      this, tr("Locate %1").arg(name), m_lastSearchFolder);
  if(picked.isEmpty())
    return;

  m_lastSearchFolder = QFileInfo{picked}.absolutePath();
  m_resolutions.insert(stored, Resolution{{picked}, 0});
  item->setText((int)MissingColumn::Found, picked);
  item->setToolTip((int)MissingColumn::Found, picked);

  updateSummary();
}

void MissingFilesDialog::applyRelink()
{
  if(!m_doc)
  {
    close();
    return;
  }

  QHash<QString, QString> chosen;
  for(auto it = m_resolutions.constBegin(); it != m_resolutions.constEnd(); ++it)
    if(it->chosen >= 0 && itemFor(it.key()))
      chosen.insert(it.key(), it->candidates[it->chosen]);

  if(chosen.isEmpty())
    return;

  const auto report = relinkFiles(m_doc->context(), chosen);

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
