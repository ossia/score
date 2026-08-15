#include <Process/MediaTrimDialog.hpp>

#include <score/document/DocumentContext.hpp>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace Process
{

MediaTrimDialog::MediaTrimDialog(const score::DocumentContext& ctx, QWidget* parent)
    : QDialog{parent}
    , m_ctx{ctx}
{
  setWindowTitle(tr("Trim media to what is used"));
  resize(900, 540);

  auto lay = new QVBoxLayout{this};

  m_summary = new QLabel{this};
  m_summary->setWordWrap(true);
  lay->addWidget(m_summary);

  auto form = new QFormLayout;
  lay->addLayout(form);

  m_handles = new QDoubleSpinBox{this};
  m_handles->setRange(0., 60.);
  m_handles->setDecimals(1);
  m_handles->setSingleStep(0.5);
  m_handles->setSuffix(tr(" s"));
  m_handles->setValue(TrimOptions{}.handles);
  m_handles->setToolTip(
      tr("Extra audio kept on each side of what the document reads, so there is "
         "still something to pull a fade out of afterwards."));
  form->addRow(tr("Keep extra"), m_handles);

  m_removeOriginal
      = new QCheckBox{tr("Delete the untrimmed files afterwards"), this};
  m_removeOriginal->setToolTip(
      tr("Leave this off unless disk space is the problem right now. Untrimmed "
         "files that stay behind are no longer referenced, so they do not travel "
         "with the project when it is archived, undo can still fall back on them, "
         "and Remove unused files can clear them out later once the edit has "
         "settled."));
  form->addRow(m_removeOriginal);

  m_files = new FileReportView{this};
  lay->addWidget(m_files, 1);

  auto buttons
      = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this};
  m_ok = buttons->button(QDialogButtonBox::Ok);
  m_ok->setText(tr("Trim"));
  lay->addWidget(buttons);

  connect(buttons, &QDialogButtonBox::accepted, this, &MediaTrimDialog::run);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(
      m_handles, &QDoubleSpinBox::valueChanged, this, [this] { reanalyze(); });
  connect(m_removeOriginal, &QCheckBox::toggled, this, [this] { reanalyze(); });

  reanalyze();
}

MediaTrimDialog::~MediaTrimDialog() = default;

TrimOptions MediaTrimDialog::options() const noexcept
{
  TrimOptions opts;
  opts.handles = m_handles->value();
  opts.removeOriginal = m_removeOriginal->isChecked();
  return opts;
}

void MediaTrimDialog::fill(const FileReport& report)
{
  // Unchanged and unsupported rows are noise here: what matters is what will
  // shrink, and what will not and why.
  m_files->setReport(
      report, {FileAction::Unchanged, FileAction::Unsupported, FileAction::AlreadyThere});

  const int trimmed = report.count(FileAction::Trimmed);
  const int skipped = report.count(FileAction::Skipped);

  QString text = tr("<b>%1 file(s) would be trimmed</b>, saving %2.")
                     .arg(trimmed)
                     .arg(QLocale{}.formattedDataSize(report.bytesSaved()));
  if(skipped > 0)
    text += tr("<br/>%1 file(s) are left alone; the reason is in the list.")
                .arg(skipped);

  if(m_removeOriginal->isChecked() && trimmed > 0)
    text += tr("<br/><b>The %1 untrimmed file(s) will be deleted.</b> This cannot "
               "be undone: undo puts the references back, but the audio is gone.")
                .arg(trimmed);
  else if(trimmed > 0)
    text += tr("<br/>The untrimmed files stay in the project folder. Nothing "
               "points at them any more, so archiving leaves them out and "
               "<i>Remove unused files</i> will offer to clear them away — "
               "until then, undo can fall back on them.");

  m_summary->setText(text);
  m_ok->setEnabled(trimmed > 0);
}

void MediaTrimDialog::reanalyze()
{
  fill(analyzeMediaTrim(m_ctx, options()));
}

void MediaTrimDialog::run()
{
  const auto opts = options();

  if(opts.removeOriginal)
  {
    const auto plan = analyzeMediaTrim(m_ctx, opts);
    const int trimmed = plan.count(FileAction::Trimmed);
    const auto res = QMessageBox::warning(
        this, tr("Delete the untrimmed files?"),
        tr("%1 audio file(s) in the project folder will be replaced by shorter "
           "ones and then deleted.\n\n"
           "The audio outside the region the document reads will be gone for "
           "good. Undo restores the references, not the files.\n\n"
           "Continue?")
            .arg(trimmed),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if(res != QMessageBox::Yes)
      return;
  }

  m_result = trimProjectMedia(m_ctx, opts);
  fill(m_result);

  if(const int failed = m_result.count(FileAction::Failed); failed > 0)
  {
    QString detail;
    for(const auto* e : m_result.with(FileAction::Failed))
      detail += e->note + '\n';

    QMessageBox::warning(
        this, tr("Trimming incomplete"),
        tr("%1 file(s) could not be trimmed; they were left as they were:\n\n%2")
            .arg(failed)
            .arg(detail));
  }

  accept();
}
}
