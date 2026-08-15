#include <Process/ProjectConsolidation.hpp>

#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <QFileInfo>

namespace Process
{
namespace
{
//! Decides what happens to each reference, and — unless this is a dry run —
//! does it.
struct Consolidator
{
  ProjectTarget target;
  score::ConsolidateOptions opts;
  score::FilePlacement placement;
  bool dryRun{};

  QString operator()(const ExternalFileRef& ref, FileEntry& e)
  {
    const auto done = [&](FileAction a, QString note = {}) {
      e.action = a;
      e.note = std::move(note);
      return QString{};
    };

    // A folder, or something score has no way to relocate (a plug-in binary).
    // Reported so the user knows what the other machine needs.
    if(!ref.rewritable || ref.directory)
      return done(FileAction::Unsupported);

    e.sourcePath = score::locateFilePath(ref.path, target.sourceRoots);

    // Files the process writes: there is nothing to collect, but the
    // destination should still follow the project so a recording made on the
    // other machine lands next to the document.
    if(ref.usage == FileUsage::Output)
    {
      e.newStoredPath = score::relativizeFilePath(e.sourcePath, target.destinationRoots);
      e.action = FileAction::AlreadyThere;
      return e.newStoredPath != e.storedPath ? e.newStoredPath : QString{};
    }

    // The user library is expected to be installed on the target machine, so
    // "<LIBRARY>:" references stay as they are unless asked otherwise.
    if(score::isLibraryRelativePath(ref.path) && !opts.collectLibraryFiles)
      return done(FileAction::KeptInLibrary);

    if(e.sourcePath.isEmpty() || !QFileInfo::exists(e.sourcePath))
      return done(FileAction::Missing);

    const auto placed = placement.place(e.sourcePath, e.kind);
    e.destinationPath = placed.destination;
    e.size = QFileInfo{e.sourcePath}.size();
    e.copyNeeded = !placed.alreadyInProject && !placed.reused;

    if(e.copyNeeded && !dryRun)
    {
      QString error;
      if(!score::materializeFile(e.sourcePath, e.destinationPath, opts.mode, error))
      {
        // Leave the reference pointing at a file that exists rather than at a
        // destination we failed to create.
        return done(FileAction::Failed, std::move(error));
      }
    }

    e.newStoredPath
        = score::relativizeFilePath(e.destinationPath, target.destinationRoots);
    e.action
        = placed.alreadyInProject ? FileAction::AlreadyThere : FileAction::Collect;

    return e.newStoredPath != e.storedPath ? e.newStoredPath : QString{};
  }
};

FileReport runConsolidation(
    const score::DocumentContext& ctx, const score::ConsolidateOptions& opts,
    QString projectFolder, bool dryRun)
{
  auto target = projectTarget(ctx, std::move(projectFolder));
  const QString folder = target.folder;

  Consolidator consolidator{
      .target = std::move(target),
      .opts = opts,
      .placement = score::FilePlacement{folder, opts},
      .dryRun = dryRun};

  auto report = runFileOperation(
      ctx,
      [&consolidator](const ExternalFileRef& r, FileEntry& e) {
    return consolidator(r, e);
      },
      dryRun);

  report.projectFolder = folder;
  return report;
}
}

ProjectTarget projectTarget(const score::DocumentContext& ctx, QString projectFolder)
{
  ProjectTarget out;
  out.sourceRoots = score::pathRoots(ctx);

  if(projectFolder.isEmpty())
    projectFolder = out.sourceRoots.documentFolder();

  if(!projectFolder.isEmpty())
  {
    const QFileInfo fi{projectFolder};
    const auto canonical = fi.canonicalFilePath();
    out.folder = canonical.isEmpty() ? fi.absoluteFilePath() : canonical;
  }

  out.destinationRoots = out.sourceRoots;
  const QString name = QFileInfo{out.sourceRoots.documentFile}.fileName();
  out.destinationRoots.documentFile
      = out.folder + '/'
        + (name.isEmpty() ? QStringLiteral("untitled.score") : name);

  return out;
}

FileReport analyzeProjectFiles(
    const score::DocumentContext& ctx, const score::ConsolidateOptions& opts,
    QString projectFolder)
{
  return runConsolidation(ctx, opts, std::move(projectFolder), /*dryRun=*/true);
}

FileReport consolidateProjectFiles(
    const score::DocumentContext& ctx, const score::ConsolidateOptions& opts,
    QString projectFolder)
{
  return runConsolidation(ctx, opts, std::move(projectFolder), /*dryRun=*/false);
}

int countProjectRelativeFiles(const score::DocumentContext& ctx)
{
  int n = 0;
  runFileOperation(
      ctx,
      [&n](const ExternalFileRef& ref, FileEntry&) {
    if(score::isProjectRelativePath(ref.path))
      ++n;
    return QString{};
      },
      /*dryRun=*/true);
  return n;
}

int reanchorProjectFiles(const score::DocumentContext& ctx)
{
  const auto roots = score::pathRoots(ctx);

  int n = 0;
  const auto report = runFileOperation(
      ctx,
      [&](const ExternalFileRef& ref, FileEntry& e) -> QString {
    if(!score::isProjectRelativePath(ref.path))
      return {};

    e.sourcePath = score::locateFilePath(ref.path, roots);
    if(e.sourcePath.isEmpty())
      return {};

    e.action = FileAction::AlreadyThere;
    e.newStoredPath = e.sourcePath;
    ++n;
    return e.sourcePath;
      },
      /*dryRun=*/false);
  return n;
}
}
