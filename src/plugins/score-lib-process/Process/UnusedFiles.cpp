#include <Process/MissingFiles.hpp>
#include <Process/ProjectConsolidation.hpp>
#include <Process/UnusedFiles.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/tools/File.hpp>

#include <core/document/Document.hpp>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <algorithm>

namespace Process
{
namespace
{
//! Extensions score itself saves documents as. Never proposed for removal,
//! whatever folder they turn up in.
bool isScoreDocument(const QString& path) noexcept
{
  const QString suffix = QFileInfo{path}.suffix().toLower();
  return suffix == QStringLiteral("score") || suffix == QStringLiteral("scorebin")
         || suffix == QStringLiteral("scorejson");
}

//! What the document points at right now.
struct UsedFiles
{
  //! Individual files, by every spelling they resolve to.
  QSet<QString> files;

  /** Folders a reference points at as a whole.
   *
   * A Faust process names the folder its `import(...)` statements resolve
   * against; an avendish folder port names a folder of data. Nothing in the
   * document names the files inside them, so without this they would all look
   * unused -- and this is the one place in score that deletes.
   */
  QStringList folders;

  bool covers(const QString& path, const QString& canonical) const noexcept
  {
    if(files.contains(path) || (!canonical.isEmpty() && files.contains(canonical)))
      return true;

    for(const QString& folder : folders)
      if(score::isUnderFolder(path, folder)
         || (!canonical.isEmpty() && score::isUnderFolder(canonical, folder)))
        return true;
    return false;
  }
};

UsedFiles usedFiles(const score::DocumentContext& ctx, const ProjectTarget& target)
{
  UsedFiles used;
  runFileOperation(
      ctx,
      [&](const ExternalFileRef& ref, FileEntry&) -> QString {
    const QString abs = score::locateFilePath(ref.path, target.sourceRoots);
    if(abs.isEmpty())
      return {};

    const QString canonical = QFileInfo{abs}.canonicalFilePath();
    if(ref.directory || QFileInfo{abs}.isDir())
    {
      used.folders.push_back(canonical.isEmpty() ? abs : canonical);
      return {};
    }

    used.files.insert(abs);
    // A reference can resolve through a symlink; both spellings count as used
    // so that neither gets swept away.
    if(!canonical.isEmpty())
      used.files.insert(canonical);
    return {};
      },
      /*dryRun=*/true);
  return used;
}

//! The folders consolidation puts things in, in this project.
QStringList collectedFolders(const QString& projectFolder)
{
  using K = score::FileKind;
  static constexpr K kinds[]
      = {K::Audio,  K::Video,  K::Image, K::Midi,   K::Model3D,
         K::Shader, K::Script, K::Data,  K::Folder, K::Plugin,
         K::Unknown};

  QStringList out;
  for(const auto k : kinds)
  {
    const QString folder = projectFolder + '/' + score::mediaSubfolder(k);
    if(QDir{folder}.exists())
      out.push_back(folder);
  }
  return out;
}
}

QString unusedFolderName() noexcept
{
  return QStringLiteral("Unused");
}

FileReport
analyzeUnusedFiles(const score::DocumentContext& ctx, const UnusedFilesOptions& opts)
{
  FileReport report;

  const auto target = projectTarget(ctx);
  report.projectFolder = target.folder;
  if(target.folder.isEmpty())
    return report;

  const UsedFiles used = usedFiles(ctx, target);
  const QString aside = target.folder + '/' + unusedFolderName();

  const QStringList roots = opts.onlyCollectedFolders
                                ? collectedFolders(target.folder)
                                : QStringList{target.folder};

  for(const QString& root : roots)
  {
    QDirIterator it{
        root, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden,
        QDirIterator::Subdirectories};

    while(it.hasNext())
    {
      const QString path = it.next();

      // Things already set aside are not found again on the next run.
      if(score::isUnderFolder(path, aside))
        continue;
      // Never a document -- not this one, not a neighbour's.
      if(isScoreDocument(path))
        continue;

      if(used.covers(path, QFileInfo{path}.canonicalFilePath()))
        continue;

      FileEntry e;
      e.sourcePath = path;
      // Shown, and identified, by where it sits in the project.
      e.storedPath = path.mid(target.folder.size() + 1);
      e.kind = score::guessFileKind(path);
      e.action = FileAction::Unused;
      e.size = QFileInfo{path}.size();
      report.entries.push_back(std::move(e));
    }
  }

  // Biggest first: whoever runs this is looking at a full disk.
  std::sort(
      report.entries.begin(), report.entries.end(),
      [](const FileEntry& a, const FileEntry& b) { return a.size > b.size; });

  return report;
}

QStringList
unusedFilesWarnings(const score::DocumentContext& ctx, const FileReport& scan)
{
  QStringList out;
  if(scan.projectFolder.isEmpty())
    return out;

  // A reference score cannot resolve is a reference whose file it cannot
  // recognise either. Sweeping now can take away the very file the user was
  // about to relink to.
  if(const int missing = scanMissingFiles(ctx).count(FileAction::Missing);
     missing > 0)
  {
    out << QObject::tr(
        "%1 file(s) referenced by this project cannot be found. Anything you "
        "were about to relink them to may be in this list.")
               .arg(missing);
  }

  // Two documents sharing a folder means each one calls the other's media
  // unused.
  QStringList documents;
  {
    QDirIterator it{
        scan.projectFolder, QStringList{"*.score", "*.scorebin", "*.scorejson"},
        QDir::Files};
    while(it.hasNext())
      documents << QFileInfo{it.next()}.fileName();
  }
  if(documents.size() > 1)
  {
    out << QObject::tr(
        "This folder holds %1 score documents (%2). Files used only by the "
        "others are in this list.")
               .arg(documents.size())
               .arg(documents.join(QStringLiteral(", ")));
  }

  // Undo does not put files back.
  if(!scan.empty())
  {
    out << QObject::tr(
        "Undo history is not taken into account: a file a previous edit used, "
        "and redo would need again, counts as unused.");
  }

  return out;
}

FileReport removeUnusedFiles(
    const score::DocumentContext& ctx, const std::vector<QString>& absolutePaths,
    const UnusedFilesOptions& opts)
{
  FileReport report;

  const auto target = projectTarget(ctx);
  report.projectFolder = target.folder;
  if(target.folder.isEmpty())
    return report;

  const QString aside = target.folder + '/' + unusedFolderName();
  const UsedFiles used = usedFiles(ctx, target);

  for(const QString& path : absolutePaths)
  {
    FileEntry e;
    e.sourcePath = path;
    e.storedPath = score::isUnderFolder(path, target.folder)
                       ? path.mid(target.folder.size() + 1)
                       : path;
    e.kind = score::guessFileKind(path);
    e.size = QFileInfo{path}.size();

    const auto refuse = [&](QString why) {
      e.action = FileAction::Skipped;
      e.note = std::move(why);
      report.entries.push_back(std::move(e));
    };

    // Three guards that do not trust the caller's list, because this is the
    // one place in score's file handling that destroys a file outright.
    if(!score::isUnderFolder(path, target.folder))
    {
      refuse(QObject::tr("outside the project folder"));
      continue;
    }
    if(isScoreDocument(path))
    {
      refuse(QObject::tr("this is a score document"));
      continue;
    }
    if(used.covers(path, QFileInfo{path}.canonicalFilePath()))
    {
      refuse(QObject::tr("the project uses this file"));
      continue;
    }
    if(!QFileInfo{path}.isFile())
    {
      refuse(QObject::tr("not a file"));
      continue;
    }

    if(opts.disposal == UnusedDisposal::Delete)
    {
      if(!QFile::remove(path))
      {
        e.action = FileAction::Failed;
        e.note = QObject::tr("could not delete %1").arg(path);
        report.entries.push_back(std::move(e));
        continue;
      }
      e.action = FileAction::Removed;
      report.entries.push_back(std::move(e));
      continue;
    }

    // Move aside, keeping the file's place in the tree so it is obvious where
    // it came from if it has to go back.
    const QString destination = aside + '/' + e.storedPath;
    if(!QDir{}.mkpath(QFileInfo{destination}.absolutePath()))
    {
      e.action = FileAction::Failed;
      e.note = QObject::tr("could not create %1").arg(QFileInfo{destination}.path());
      report.entries.push_back(std::move(e));
      continue;
    }

    const QString target_path = score::addUniqueSuffix(destination);
    if(!QFile::rename(path, target_path))
    {
      e.action = FileAction::Failed;
      e.note = QObject::tr("could not move %1 aside").arg(path);
      report.entries.push_back(std::move(e));
      continue;
    }

    e.destinationPath = target_path;
    e.action = FileAction::Removed;
    report.entries.push_back(std::move(e));
  }

  return report;
}
}
