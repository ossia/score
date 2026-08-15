#include <Process/MediaTrimDialog.hpp>
#include <Process/MissingFilesDialog.hpp>
#include <Process/ProjectArchive.hpp>
#include <Process/UnusedFilesDialog.hpp>
#include <Process/ProjectConsolidation.hpp>
#include <Process/ProjectConsolidationDialog.hpp>
#include <Process/ProjectFilesApplicationPlugin.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/actions/MenuManager.hpp>
#include <score/tools/Zip.hpp>
#include <score/widgets/HelpInteraction.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QLocale>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTimer>

SCORE_DECLARE_ACTION(
    ConsolidateProject, "&Consolidate project...", Common, QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(
    LocateMissingFiles, "&Locate missing files...", Common, QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(
    TrimProjectMedia, "&Shorten media files to what is played...", Common,
    QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(
    RemoveUnusedFiles, "&Remove unused files...", Common, QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(
    ArchiveProject, "&Archive project...", Common, QKeySequence::UnknownKey)

namespace Process
{

ProjectFilesApplicationPlugin::ProjectFilesApplicationPlugin(
    const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}
{
}

ProjectFilesApplicationPlugin::~ProjectFilesApplicationPlugin() = default;

score::Document*
ProjectFilesApplicationPlugin::documentWithFolder(const QString& what)
{
  auto doc = context.docManager.currentDocument();
  if(!doc)
    return nullptr;

  // Every path this writes is relative to the document's own folder, so there
  // has to be one. This is the "save your set into its own project folder
  // first" rule every DAW ends up teaching its users; ask for it up front
  // rather than silently working in the wrong place.
  if(doc->metadata().projectFolder().isEmpty())
  {
    const auto res = QMessageBox::question(
        context.mainWindow, QObject::tr("Save the document first"),
        QObject::tr("%1 works next to the document, so the document has to be "
                    "saved somewhere first.\n\nSave it now?")
            .arg(what),
        QMessageBox::Save | QMessageBox::Cancel);
    if(res != QMessageBox::Save)
      return nullptr;
    if(!context.docManager.saveDocumentAs(*doc))
      return nullptr;
    if(doc->metadata().projectFolder().isEmpty())
      return nullptr;
  }

  return doc;
}

void ProjectFilesApplicationPlugin::consolidate()
{
  auto doc = documentWithFolder(QObject::tr("Consolidating"));
  if(!doc)
    return;

  ProjectConsolidationDialog dialog{doc->context(), context.mainWindow};
  if(dialog.exec() != QDialog::Accepted)
    return;

  // Save right away: the copies on disk and the rewritten references only
  // make sense together, and a document closed without saving would leave
  // the collected files orphaned.
  context.docManager.saveDocument(*doc);
}

void ProjectFilesApplicationPlugin::locateMissingFiles()
{
  auto doc = context.docManager.currentDocument();
  if(!doc)
    return;

  auto* dialog = new MissingFilesDialog{doc->context(), context.mainWindow};
  dialog->show();
}

void ProjectFilesApplicationPlugin::trimMedia()
{
  auto doc = documentWithFolder(QObject::tr("Trimming"));
  if(!doc)
    return;

  MediaTrimDialog dialog{doc->context(), context.mainWindow};
  if(dialog.exec() != QDialog::Accepted)
    return;

  context.docManager.saveDocument(*doc);
}

void ProjectFilesApplicationPlugin::removeUnused()
{
  auto doc = documentWithFolder(QObject::tr("Cleaning up"));
  if(!doc)
    return;

  UnusedFilesDialog dialog{doc->context(), context.mainWindow};
  dialog.exec();
}

void ProjectFilesApplicationPlugin::archive()
{
  auto doc = documentWithFolder(QObject::tr("Archiving"));
  if(!doc)
    return;

  const QString suggested
      = doc->metadata().projectFolder() + '/' + doc->metadata().documentName()
        + QStringLiteral(".zip");
  const QString destination = QFileDialog::getSaveFileName(
      context.mainWindow, QObject::tr("Archive project as"), suggested,
      QObject::tr("Archives (*.zip)"));
  if(destination.isEmpty())
    return;

  // Collect first: an archive of a project whose media is scattered over the
  // machine would be an archive of nothing.
  const auto report = consolidateProjectFiles(doc->context(), {});
  context.docManager.saveDocument(*doc);

  const auto contents = projectArchiveContents(doc->context(), report);
  if(contents.empty())
  {
    QMessageBox::warning(
        context.mainWindow, QObject::tr("Nothing to archive"),
        QObject::tr("No file of this project could be found on disk."));
    return;
  }

  QProgressDialog progress{
      QObject::tr("Archiving %1 file(s), %2...")
          .arg(contents.size())
          .arg(QLocale{}.formattedDataSize(archiveContentsSize(contents))),
      QObject::tr("Cancel"), 0, int(contents.size()), context.mainWindow};
  progress.setWindowModality(Qt::WindowModal);

  QString error;
  const bool ok = score::writeZipArchive(
      destination, contents, /*level=*/1, error, [&](int done, int total) {
    progress.setValue(done);
    QCoreApplication::processEvents();
    return !progress.wasCanceled();
      });

  progress.close();

  if(!ok)
  {
    QMessageBox::warning(context.mainWindow, QObject::tr("Archiving failed"), error);
    return;
  }

  const int missing = report.count(FileAction::Missing);
  const int external = report.count(FileAction::Unsupported);
  QString note = QObject::tr("%1 holds %2 file(s).")
                     .arg(QFileInfo{destination}.fileName())
                     .arg(contents.size());
  if(missing > 0)
    note += QObject::tr("\n\n%1 file(s) could not be found and are NOT in the "
                        "archive.")
                .arg(missing);
  if(external > 0)
    note += QObject::tr("\n\n%1 external dependency/ies (plug-ins, folders) must "
                        "be installed on the other machine.")
                .arg(external);

  QMessageBox::information(context.mainWindow, QObject::tr("Project archived"), note);
}

void ProjectFilesApplicationPlugin::on_loadedDocument(score::Document& doc)
{
  if(!context.mainWindow)
    return;

  // The document is still being built: look at it once the load has settled,
  // and never block it.
  QPointer<score::Document> guard{&doc};
  QTimer::singleShot(0, this, [this, guard] {
    if(!guard)
      return;
    if(MissingFilesDialog::nothingMissing(guard->context()))
      return;

    auto* dialog = new MissingFilesDialog{guard->context(), context.mainWindow};
    dialog->show();
  });
}

void ProjectFilesApplicationPlugin::on_documentSaveAs(
    score::Document& doc, const QString& newFileName)
{
  const QString before = doc.metadata().projectFolder();
  if(before.isEmpty())
    return; // never saved: nothing was relative to anything yet

  const QString after = QFileInfo{newFileName}.absolutePath();
  if(before == after || QFileInfo{after}.canonicalFilePath() == before)
    return; // same folder: the relative references still mean what they meant

  const auto& ctx = doc.context();
  const int relative = countProjectRelativeFiles(ctx);
  if(relative == 0)
    return;

  // Saving elsewhere silently turns every "<PROJECT>:..." into a file that is
  // not there. Neither outcome is obviously right, so ask; but never leave the
  // document in the broken third state.
  // Without a window to ask in (scripted saves), re-anchor: copying gigabytes
  // of media is not something to do behind a script's back, and the document
  // still works afterwards.
  auto choice = QMessageBox::No;
  if(context.mainWindow)
  {
    choice = QMessageBox::StandardButton(QMessageBox::question(
        context.mainWindow, QObject::tr("Media of this project"),
        QObject::tr("This project is being saved to a different folder, and %1 of "
                    "its file(s) live next to the previous one.\n\n"
                    "Copy them into the new folder?\n\n"
                    "Choosing No keeps them where they are and points the project "
                    "at them with absolute paths.")
            .arg(relative),
        QMessageBox::Yes | QMessageBox::No));
  }

  if(choice == QMessageBox::Yes)
    consolidateProjectFiles(ctx, {}, after);
  else
    reanchorProjectFiles(ctx);
}

score::GUIElements ProjectFilesApplicationPlugin::makeGUIElements()
{
  score::GUIElements e;
  if(!context.mainWindow)
    return e;

  // Five entries is too many to leave loose in the File menu, and two of them
  // -- shortening files and removing files -- read as near-synonyms until they
  // are seen side by side. Their own submenu does both jobs.
  auto* project = new QMenu{QObject::tr("Project &files")};
  e.menus.emplace_back(project, score::Menus::ProjectFiles());

  auto& file = context.menus.get().at(score::Menus::File());
  auto& cond = context.actions.condition<score::EnableActionIfDocument>();

  {
    // A plug-in's additions land at the end of the menu, which here is after
    // Quit. Go in above the last separator instead, beside Save As.
    QAction* before = nullptr;
    const auto existing = file.menu()->actions();
    for(int i = existing.size() - 1; i >= 0; --i)
    {
      if(existing[i]->isSeparator())
      {
        before = existing[i];
        break;
      }
    }

    if(before)
      file.menu()->insertMenu(before, project);
    else
      file.menu()->addMenu(project);
  }

  const auto add = [&](QAction*& action, auto&& slot, const QString& help) {
    action = new QAction{context.mainWindow};
    score::setHelp(action, help);
    connect(action, &QAction::triggered, this, slot);
    project->addAction(action);
  };

  add(m_consolidate, &ProjectFilesApplicationPlugin::consolidate,
      QObject::tr("Copy every file the document uses next to it, and make the "
                  "references relative to the project."));
  e.actions.add<Actions::ConsolidateProject>(m_consolidate);
  cond.add<Actions::ConsolidateProject>();

  add(m_locate, &ProjectFilesApplicationPlugin::locateMissingFiles,
      QObject::tr("List the files this document cannot find and help point it at "
                  "them."));
  e.actions.add<Actions::LocateMissingFiles>(m_locate);
  cond.add<Actions::LocateMissingFiles>();

  project->addSeparator();

  add(m_unused, &ProjectFilesApplicationPlugin::removeUnused,
      QObject::tr("List the files sitting in the project folder that nothing in "
                  "the document uses any more, and get rid of them."));
  e.actions.add<Actions::RemoveUnusedFiles>(m_unused);
  cond.add<Actions::RemoveUnusedFiles>();

  add(m_trim, &ProjectFilesApplicationPlugin::trimMedia,
      QObject::tr("Shorten media files that are used down to the parts that are "
                  "played. Whole files nothing uses are removed by the entry "
                  "above instead."));
  e.actions.add<Actions::TrimProjectMedia>(m_trim);
  cond.add<Actions::TrimProjectMedia>();

  project->addSeparator();

  add(m_archive, &ProjectFilesApplicationPlugin::archive,
      QObject::tr("Collect everything and write the whole project into a single "
                  "zip file."));
  e.actions.add<Actions::ArchiveProject>(m_archive);
  cond.add<Actions::ArchiveProject>();

  return e;
}
}
