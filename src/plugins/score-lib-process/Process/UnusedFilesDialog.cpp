#include <Process/UnusedFilesDialog.hpp>

#include <score/document/DocumentContext.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace Process
{

UnusedFilesDialog::UnusedFilesDialog(const score::DocumentContext& ctx, QWidget* parent)
    : QDialog{parent}
    , m_ctx{ctx}
{
  setWindowTitle(tr("Unused files"));
  resize(900, 560);

  auto lay = new QVBoxLayout{this};

  m_summary = new QLabel{this};
  m_summary->setWordWrap(true);
  lay->addWidget(m_summary);

  m_warnings = new QLabel{this};
  m_warnings->setWordWrap(true);
  m_warnings->setTextFormat(Qt::RichText);
  lay->addWidget(m_warnings);

  auto form = new QFormLayout;
  lay->addLayout(form);

  m_disposal = new QComboBox{this};
  m_disposal->addItem(
      tr("Move them to the Unused folder"), int(UnusedDisposal::MoveAside));
  m_disposal->addItem(tr("Delete them"), int(UnusedDisposal::Delete));
  m_disposal->setToolTip(
      tr("Moving keeps them in the project, out of the way and out of archives, "
         "so a mistake costs a drag back. Deleting does not."));
  form->addRow(tr("What to do"), m_disposal);

  m_everywhere = new QCheckBox{
      tr("Look through the whole project folder, not just Audio/, Video/, ..."),
      this};
  m_everywhere->setToolTip(
      tr("Off by default. score put the files in those folders and knows what "
         "they were for; the rest of a project folder can hold renders, notes "
         "and other people's work."));
  form->addRow(m_everywhere);

  m_files = new FileReportView{this};
  m_files->setCheckable(true);
  lay->addWidget(m_files, 1);

  auto buttons
      = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this};
  m_ok = buttons->button(QDialogButtonBox::Ok);
  lay->addWidget(buttons);

  connect(buttons, &QDialogButtonBox::accepted, this, &UnusedFilesDialog::run);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_everywhere, &QCheckBox::toggled, this, [this] { rescan(); });
  connect(m_disposal, &QComboBox::currentIndexChanged, this, [this] { rescan(); });

  rescan();
}

UnusedFilesDialog::~UnusedFilesDialog() = default;

UnusedFilesOptions UnusedFilesDialog::options() const noexcept
{
  return {
      .onlyCollectedFolders = !m_everywhere->isChecked(),
      .disposal = UnusedDisposal(m_disposal->currentData().toInt())};
}

void UnusedFilesDialog::rescan()
{
  m_scan = analyzeUnusedFiles(m_ctx, options());
  m_files->setReport(m_scan);

  const bool deleting = options().disposal == UnusedDisposal::Delete;
  m_ok->setText(deleting ? tr("Delete") : tr("Move aside"));
  m_ok->setEnabled(!m_scan.empty());

  qint64 total = 0;
  for(const auto& e : m_scan.entries)
    total += e.size;

  const QString scope = m_everywhere->isChecked()
                            ? tr("the whole project folder")
                            : tr("the Audio/, Video/, Images/... folders");

  if(m_scan.empty())
  {
    m_summary->setText(
        tr("<b>%1</b><br/>Nothing unused was found in %2.")
            .arg(m_scan.projectFolder)
            .arg(scope));
  }
  else
  {
    m_summary->setText(
        tr("<b>%1</b><br/>%2 file(s) in %3, %4, that nothing in this project "
           "points at. Untick anything you want to keep.")
            .arg(m_scan.projectFolder)
            .arg(m_scan.entries.size())
            .arg(scope)
            .arg(QLocale{}.formattedDataSize(total)));
  }

  const auto warnings = unusedFilesWarnings(m_ctx, m_scan);
  if(warnings.isEmpty())
  {
    m_warnings->clear();
    m_warnings->hide();
  }
  else
  {
    m_warnings->setText(
        tr("<b>Before you do:</b><ul><li>%1</li></ul>")
            .arg(warnings.join(QStringLiteral("</li><li>"))));
    m_warnings->show();
  }
}

void UnusedFilesDialog::run()
{
  // Only what is ticked, resolved back to the files the scan found.
  const auto ticked = m_files->checkedPaths();
  std::vector<QString> paths;
  for(const auto& stored : ticked)
    for(const auto& e : m_scan.entries)
      if(e.storedPath == stored)
        paths.push_back(e.sourcePath);

  if(paths.empty())
  {
    reject();
    return;
  }

  const auto opts = options();
  if(opts.disposal == UnusedDisposal::Delete)
  {
    qint64 total = 0;
    for(const auto& e : m_scan.entries)
      if(std::find(paths.begin(), paths.end(), e.sourcePath) != paths.end())
        total += e.size;

    const auto res = QMessageBox::warning(
        this, tr("Delete these files?"),
        tr("%1 file(s), %2, will be deleted from %3.\n\n"
           "This cannot be undone -- score's undo does not restore files.\n\n"
           "Continue?")
            .arg(paths.size())
            .arg(QLocale{}.formattedDataSize(total))
            .arg(m_scan.projectFolder),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if(res != QMessageBox::Yes)
      return;
  }

  m_result = removeUnusedFiles(m_ctx, paths, opts);

  if(const int failed = m_result.count(FileAction::Failed)
                        + m_result.count(FileAction::Skipped);
     failed > 0)
  {
    QString detail;
    for(const auto& e : m_result.entries)
      if(!e.note.isEmpty())
        detail += e.storedPath + ": " + e.note + '\n';

    QMessageBox::warning(
        this, tr("Some files were left alone"),
        tr("%1 file(s) were not removed:\n\n%2").arg(failed).arg(detail));
  }

  accept();
}
}
