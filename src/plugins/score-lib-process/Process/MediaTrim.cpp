#include <Process/MediaTrim.hpp>
#include <Process/MediaTrimmer.hpp>
#include <Process/ProjectConsolidation.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/File.hpp>

#include <QFile>
#include <QFileInfo>
#include <QHash>

#include <algorithm>

namespace Process
{
namespace
{
//! Union of every region that reads a given file, keyed by absolute path.
using RangeMap = QHash<QString, MediaRange>;

void addRange(RangeMap& map, const QString& file, MediaRange r)
{
  const auto it = map.find(file);
  if(it == map.end())
  {
    map.insert(file, r);
    return;
  }

  const double start = std::min(it->start, r.start);
  const double end = std::max(it->end(), r.end());
  *it = MediaRange{start, end - start};
}

//! Collect, over the whole document, the region of each file that is read.
//! A file any reference cannot bound is marked unbounded and never trimmed.
struct RangeScan
{
  RangeMap ranges;
  QHash<QString, QString> refused; //!< file -> why it cannot be trimmed
};

RangeScan scanRanges(const score::DocumentContext& ctx, const ProjectTarget& target)
{
  RangeScan out;
  runFileOperation(
      ctx,
      [&](const ExternalFileRef& ref, FileEntry&) -> QString {
    if(!ref.rewritable || ref.directory || ref.usage == FileUsage::Output)
      return {};

    const QString abs = score::locateFilePath(ref.path, target.sourceRoots);
    if(abs.isEmpty())
      return {};

    if(!ref.usedRange)
    {
      out.refused.insert(
          abs, QObject::tr("%1 reads an unbounded region").arg(ref.owner));
      return {};
    }

    addRange(out.ranges, abs, *ref.usedRange);
    return {};
      },
      /*dryRun=*/true);

  // One unbounded reader is enough to disqualify the file for every reader.
  for(const auto& file : out.refused.keys())
    out.ranges.remove(file);

  return out;
}

//! A free name beside `source`, with the trimmer's extension.
QString trimDestination(const QString& source, const QString& extension)
{
  const QFileInfo fi{source};
  const QString base = fi.absolutePath() + '/' + fi.completeBaseName();
  return score::addUniqueSuffix(base + QStringLiteral(" (trimmed).") + extension);
}

struct Trimmer
{
  ProjectTarget target;
  TrimOptions opts;
  const MediaTrimmerList& trimmers;
  RangeScan scan;
  //! Files already trimmed in this run: a file referenced twice is written
  //! once and both references land on the same result.
  QHash<QString, QString> done;
  bool dryRun{};

  QString operator()(const ExternalFileRef& ref, FileEntry& e)
  {
    const auto skip = [&](QString why) {
      e.action = FileAction::Skipped;
      e.note = std::move(why);
      return QString{};
    };

    if(!ref.rewritable || ref.directory || ref.usage == FileUsage::Output)
    {
      e.action = FileAction::Unsupported;
      return {};
    }

    e.sourcePath = score::locateFilePath(ref.path, target.sourceRoots);
    if(e.sourcePath.isEmpty() || !QFileInfo::exists(e.sourcePath))
    {
      e.action = FileAction::Missing;
      return {};
    }

    // Already handled through another reference to the same file.
    if(const auto it = done.constFind(e.sourcePath); it != done.constEnd())
    {
      e.destinationPath = *it;
      e.action = FileAction::Trimmed;
      e.size = QFileInfo{e.sourcePath}.size();
      e.newSize = QFileInfo{e.destinationPath}.size();
      e.newStoredPath
          = score::relativizeFilePath(e.destinationPath, target.destinationRoots);
      return e.newStoredPath;
    }

    // Nothing outside the project folder is ever rewritten: a document that
    // points at a shared sample library must not start editing that library.
    if(!score::isUnderFolder(e.sourcePath, target.folder))
      return skip(QObject::tr("outside the project folder"));

    if(const auto it = scan.refused.constFind(e.sourcePath);
       it != scan.refused.constEnd())
      return skip(*it);

    const auto range = scan.ranges.constFind(e.sourcePath);
    if(range == scan.ranges.constEnd())
      return skip(QObject::tr("no bounded region"));

    const auto* trimmer = trimmers.find(e.sourcePath);
    if(!trimmer)
      return skip(QObject::tr("no trimmer for this kind of file"));

    e.size = QFileInfo{e.sourcePath}.size();

    // Widen by the handles and clamp to the file.
    MediaRange kept{
        std::max(0., range->start - opts.handles), range->duration + 2 * opts.handles};

    const double fileDuration = mediaDuration(e.sourcePath);
    if(fileDuration <= 0.)
      return skip(QObject::tr("unknown duration"));

    kept.duration = std::min(kept.duration, fileDuration - kept.start);
    if(kept.duration <= 0.)
      return skip(QObject::tr("region is empty"));

    if(kept.duration >= fileDuration * opts.maximumUsedFraction)
      return skip(QObject::tr("almost all of the file is used"));

    e.destinationPath = trimDestination(e.sourcePath, trimmer->outputExtension());
    e.newStoredPath
        = score::relativizeFilePath(e.destinationPath, target.destinationRoots);

    if(dryRun)
    {
      e.newSize = trimmer->estimatedSize(e.sourcePath, kept);
      if(e.size - e.newSize < opts.minimumGain)
      {
        e.destinationPath.clear();
        e.newStoredPath.clear();
        e.newSize = 0;
        return skip(QObject::tr("would not save any space"));
      }
      e.action = FileAction::Trimmed;
      return {};
    }

    if(const QString error = trimmer->trim(e.sourcePath, e.destinationPath, kept);
       !error.isEmpty())
    {
      QFile::remove(e.destinationPath);
      e.action = FileAction::Failed;
      e.note = error;
      return {};
    }

    e.newSize = QFileInfo{e.destinationPath}.size();

    // Trimming that makes the file bigger is trimming that should not have
    // happened -- re-encoding a compressed source easily does that.
    if(e.newSize <= 0 || e.size - e.newSize < opts.minimumGain)
    {
      QFile::remove(e.destinationPath);
      e.destinationPath.clear();
      e.newStoredPath.clear();
      e.newSize = 0;
      return skip(QObject::tr("would not save any space"));
    }

    done.insert(e.sourcePath, e.destinationPath);
    e.action = FileAction::Trimmed;
    return e.newStoredPath;
  }

  //! Duration of a media file in seconds, via whichever trimmer reads it.
  double mediaDuration(const QString& path) const
  {
    if(const auto* t = trimmers.find(path))
      return t->duration(path);
    return 0.;
  }
};
}

static FileReport
runTrim(const score::DocumentContext& ctx, const TrimOptions& opts, bool dryRun)
{
  const auto target = projectTarget(ctx);

  Trimmer trimmer{
      .target = target,
      .opts = opts,
      .trimmers = ctx.app.interfaces<MediaTrimmerList>(),
      .scan = scanRanges(ctx, target),
      .dryRun = dryRun};

  auto report = runFileOperation(
      ctx,
      [&trimmer](const ExternalFileRef& r, FileEntry& e) { return trimmer(r, e); },
      dryRun);
  report.projectFolder = target.folder;

  if(!dryRun && opts.removeOriginal)
  {
    for(auto& e : report.entries)
    {
      if(e.action != FileAction::Trimmed || e.sourcePath.isEmpty())
        continue;
      if(e.sourcePath == e.destinationPath)
        continue;
      // Belt and braces: the trim policy already refuses anything outside the
      // project, and this is the one place that deletes.
      if(!score::isUnderFolder(e.sourcePath, target.folder))
        continue;
      if(QFileInfo info{e.destinationPath}; !info.isFile() || info.size() <= 0)
        continue;

      QFile::remove(e.sourcePath);
    }
  }

  return report;
}

FileReport analyzeMediaTrim(const score::DocumentContext& ctx, const TrimOptions& opts)
{
  return runTrim(ctx, opts, /*dryRun=*/true);
}

FileReport trimProjectMedia(const score::DocumentContext& ctx, const TrimOptions& opts)
{
  return runTrim(ctx, opts, /*dryRun=*/false);
}
}
