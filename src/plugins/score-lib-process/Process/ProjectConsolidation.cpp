#include <Process/Commands/RelocateFile.hpp>
#include <Process/ProjectConsolidation.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <QFileInfo>

namespace Process
{

qint64 ConsolidationReport::bytesToCopy() const noexcept
{
  qint64 total = 0;
  for(const auto& e : entries)
    if(e.copyNeeded)
      total += e.size;
  return total;
}

int ConsolidationReport::count(ConsolidationAction a) const noexcept
{
  int n = 0;
  for(const auto& e : entries)
    if(e.action == a)
      ++n;
  return n;
}

std::vector<const ConsolidationEntry*> ConsolidationReport::missing() const noexcept
{
  std::vector<const ConsolidationEntry*> out;
  for(const auto& e : entries)
    if(e.action == ConsolidationAction::Missing)
      out.push_back(&e);
  return out;
}

namespace
{
//! Decides what happens to each reference, and — unless this is a dry run —
//! does it.
struct Consolidator
{
  score::PathRoots sourceRoots;      //!< where references resolve from today
  score::PathRoots destinationRoots; //!< what the rewritten references mean
  score::ConsolidateOptions opts;
  score::FilePlacement placement;
  bool dryRun{};

  ConsolidationReport report;

  QString operator()(const ExternalFileRef& ref)
  {
    ConsolidationEntry e;
    e.storedPath = ref.path;
    e.kind = ref.kind;
    e.usage = ref.usage;
    e.owner = ref.owner;

    const auto done = [&](ConsolidationAction a) {
      e.action = a;
      report.entries.push_back(std::move(e));
      return QString{};
    };

    // An analysis asks for no rewrite at all: the report is built either way,
    // and there is no point making the processes construct (and immediately
    // throw away) relocation commands every time the user ticks an option.
    const auto rewrite = [&](const QString& to) { return dryRun ? QString{} : to; };

    // A folder, or something score has no way to relocate (a plug-in binary).
    // Reported so the user knows what the other machine needs.
    if(!ref.rewritable || ref.directory)
      return done(ConsolidationAction::Unsupported);

    e.sourcePath = score::locateFilePath(ref.path, sourceRoots);

    // Files the process writes: there is nothing to collect, but the
    // destination should still follow the project so a recording made on the
    // other machine lands next to the document.
    if(ref.usage == FileUsage::Output)
    {
      e.newStoredPath = score::relativizeFilePath(e.sourcePath, destinationRoots);
      e.action = ConsolidationAction::AlreadyThere;
      const QString next
          = e.newStoredPath != e.storedPath ? rewrite(e.newStoredPath) : QString{};
      report.entries.push_back(std::move(e));
      return next;
    }

    // The user library is expected to be installed on the target machine, so
    // "<LIBRARY>:" references stay as they are unless asked otherwise.
    if(score::isLibraryRelativePath(ref.path) && !opts.collectLibraryFiles)
      return done(ConsolidationAction::KeptInLibrary);

    if(e.sourcePath.isEmpty() || !QFileInfo::exists(e.sourcePath))
      return done(ConsolidationAction::Missing);

    const auto placed = placement.place(e.sourcePath, e.kind);
    e.destinationPath = placed.destination;
    e.size = QFileInfo{e.sourcePath}.size();
    e.copyNeeded = !placed.alreadyInProject && !placed.reused;

    if(e.copyNeeded && !dryRun)
    {
      QString error;
      if(!score::materializeFile(
             e.sourcePath, e.destinationPath, opts.mode, error))
      {
        // Leave the reference pointing at a file that exists rather than at a
        // destination we failed to create.
        e.error = std::move(error);
        return done(ConsolidationAction::Failed);
      }
    }

    e.newStoredPath = score::relativizeFilePath(e.destinationPath, destinationRoots);
    e.action = placed.alreadyInProject ? ConsolidationAction::AlreadyThere
                                       : ConsolidationAction::Collect;

    const QString next
        = e.newStoredPath != e.storedPath ? rewrite(e.newStoredPath) : QString{};
    report.entries.push_back(std::move(e));
    return next;
  }
};

QString normalizedProjectFolder(const score::PathRoots& roots, QString projectFolder)
{
  if(projectFolder.isEmpty())
    projectFolder = roots.documentFolder();
  if(projectFolder.isEmpty())
    return {};

  const QFileInfo fi{projectFolder};
  if(const auto c = fi.canonicalFilePath(); !c.isEmpty())
    return c;
  return fi.absoluteFilePath();
}

//! Roots describing the document once it lives in `projectFolder`.
score::PathRoots
destinationRootsFor(const score::PathRoots& roots, const QString& projectFolder)
{
  score::PathRoots out = roots;
  const QString name = QFileInfo{roots.documentFile}.fileName();
  out.documentFile
      = projectFolder + '/' + (name.isEmpty() ? QStringLiteral("untitled.score") : name);
  return out;
}

Consolidator makeConsolidator(
    const score::DocumentContext& ctx, const score::ConsolidateOptions& opts,
    QString projectFolder, bool dryRun)
{
  const auto roots = score::pathRoots(ctx);
  const QString folder = normalizedProjectFolder(roots, std::move(projectFolder));

  return Consolidator{
      .sourceRoots = roots,
      .destinationRoots = destinationRootsFor(roots, folder),
      .opts = opts,
      .placement = score::FilePlacement{folder, opts},
      .dryRun = dryRun};
}
}

ConsolidationReport analyzeProjectFiles(
    const score::DocumentContext& ctx, const score::ConsolidateOptions& opts,
    QString projectFolder)
{
  auto consolidator
      = makeConsolidator(ctx, opts, std::move(projectFolder), /*dryRun=*/true);
  const QString folder = consolidator.placement.projectFolder();

  ExternalFileMap map{[&consolidator](const ExternalFileRef& r) {
    return consolidator(r);
  }};
  mapDocumentExternalFiles(ctx, map);

  auto report = std::move(consolidator.report);
  report.projectFolder = folder;
  return report;
}

ConsolidationReport consolidateProjectFiles(
    const score::DocumentContext& ctx, const score::ConsolidateOptions& opts,
    QString projectFolder)
{
  auto consolidator
      = makeConsolidator(ctx, opts, std::move(projectFolder), /*dryRun=*/false);
  const QString folder = consolidator.placement.projectFolder();

  ExternalFileMap map{[&consolidator](const ExternalFileRef& r) {
    return consolidator(r);
  }};
  mapDocumentExternalFiles(ctx, map);

  if(map.hasCommands())
  {
    auto* macro = new Process::ConsolidateProjectFiles;
    for(auto* cmd : map.takeCommands())
      macro->addCommand(cmd);

    CommandDispatcher<>{ctx.commandStack}.submit(macro);
  }

  auto report = std::move(consolidator.report);
  report.projectFolder = folder;
  return report;
}

int countProjectRelativeFiles(const score::DocumentContext& ctx)
{
  int n = 0;
  ExternalFileMap map{[&n](const ExternalFileRef& ref) {
    if(score::isProjectRelativePath(ref.path))
      ++n;
    return QString{};
  }};
  mapDocumentExternalFiles(ctx, map);
  return n;
}

int reanchorProjectFiles(const score::DocumentContext& ctx)
{
  const auto roots = score::pathRoots(ctx);

  int n = 0;
  ExternalFileMap map{[&](const ExternalFileRef& ref) -> QString {
    if(!score::isProjectRelativePath(ref.path))
      return {};

    const QString absolute = score::locateFilePath(ref.path, roots);
    if(absolute.isEmpty())
      return {};

    ++n;
    return absolute;
  }};
  mapDocumentExternalFiles(ctx, map);

  if(map.hasCommands())
  {
    auto* macro = new Process::ConsolidateProjectFiles;
    for(auto* cmd : map.takeCommands())
      macro->addCommand(cmd);

    CommandDispatcher<>{ctx.commandStack}.submit(macro);
  }
  return n;
}
}
