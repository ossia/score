#include <Process/ProjectConsolidationDialog.hpp>

#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace Process
{

ProjectConsolidationDialog::ProjectConsolidationDialog(
    const score::DocumentContext& ctx, QWidget* parent)
    : QDialog{parent}
    , m_ctx{ctx}
{
  setWindowTitle(tr("Consolidate project"));
  resize(900, 520);

  auto lay = new QVBoxLayout{this};

  m_summary = new QLabel{this};
  m_summary->setWordWrap(true);
  lay->addWidget(m_summary);

  auto form = new QFormLayout;
  lay->addLayout(form);

  m_mode = new QComboBox{this};
  m_mode->addItem(tr("Copy the files"), int(score::CopyMode::Copy));
  m_mode->addItem(tr("Symbolic links"), int(score::CopyMode::Symlink));
  m_mode->addItem(tr("Hard links"), int(score::CopyMode::Hardlink));
  m_mode->setToolTip(
      tr("Only copies make the project self-contained. Links keep the disk usage "
         "down on this machine but break as soon as the project is moved."));
  form->addRow(tr("Method"), m_mode);

  m_library = new QCheckBox{tr("Also collect files from the user library"), this};
  m_library->setToolTip(
      tr("Off by default: the library is expected to be installed on the other "
         "machine, and collecting it can copy a lot of shared content."));
  form->addRow(m_library);

  m_subfolders = new QCheckBox{tr("Sort into Audio/, Video/, Images/..."), this};
  m_subfolders->setChecked(true);
  form->addRow(m_subfolders);

  m_keepFolderName = new QCheckBox{tr("Keep the name of the source folder"), this};
  m_keepFolderName->setToolTip(
      tr("Collects Kicks/kick.wav as Audio/Kicks/kick.wav, so files coming from "
         "different sample folders stay distinguishable."));
  form->addRow(m_keepFolderName);

  m_files = new FileReportView{this};
  lay->addWidget(m_files, 1);

  auto buttons
      = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this};
  m_ok = buttons->button(QDialogButtonBox::Ok);
  m_ok->setText(tr("Consolidate"));
  lay->addWidget(buttons);

  connect(buttons, &QDialogButtonBox::accepted, this, &ProjectConsolidationDialog::run);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  const auto refresh = [this] { reanalyze(); };
  connect(m_mode, &QComboBox::currentIndexChanged, this, refresh);
  connect(m_library, &QCheckBox::toggled, this, refresh);
  connect(m_subfolders, &QCheckBox::toggled, this, refresh);
  connect(m_keepFolderName, &QCheckBox::toggled, this, refresh);

  reanalyze();
}

ProjectConsolidationDialog::~ProjectConsolidationDialog() = default;

score::ConsolidateOptions ProjectConsolidationDialog::options() const noexcept
{
  return {
      .mode = score::CopyMode(m_mode->currentData().toInt()),
      .collectLibraryFiles = m_library->isChecked(),
      .useKindSubfolders = m_subfolders->isChecked(),
      .keepSourceFolderName = m_keepFolderName->isChecked()};
}

void ProjectConsolidationDialog::fill(const FileReport& report)
{
  m_files->setReport(report);

  const int collect = report.count(FileAction::Collect);
  const int missing = report.count(FileAction::Missing);
  const int external = report.count(FileAction::Unsupported);

  QString text = tr("<b>%1</b><br/>%2 file(s) to collect, %3.")
                     .arg(report.projectFolder)
                     .arg(collect)
                     .arg(QLocale{}.formattedDataSize(report.bytesToCopy()));
  if(missing > 0)
    text += tr("<br/><b>%1 file(s) cannot be found</b> and will be left alone.")
                .arg(missing);
  if(external > 0)
    text += tr("<br/>%1 external dependency/ies (plug-ins, folders) must be "
               "installed on the other machine.")
                .arg(external);
  m_summary->setText(text);

  m_ok->setEnabled(!report.empty());
}

void ProjectConsolidationDialog::reanalyze()
{
  fill(analyzeProjectFiles(m_ctx, options()));
}

void ProjectConsolidationDialog::run()
{
  m_result = consolidateProjectFiles(m_ctx, options());
  fill(m_result);

  if(const int failed = m_result.count(FileAction::Failed); failed > 0)
  {
    QString detail;
    for(const auto* e : m_result.with(FileAction::Failed))
      detail += e->note + '\n';

    QMessageBox::warning(
        this, tr("Consolidation incomplete"),
        tr("%1 file(s) could not be collected; their references were left "
           "untouched:\n\n%2")
            .arg(failed)
            .arg(detail));
  }

  accept();
}
}
