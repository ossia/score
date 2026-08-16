#include <Process/MissingFiles.hpp>
#include <Process/ProjectConsolidation.hpp>

#include <score/document/DocumentContext.hpp>

#include <QDirIterator>
#include <QFileInfo>

#include <algorithm>

namespace Process
{

FileReport scanMissingFiles(const score::DocumentContext& ctx)
{
  const auto target = projectTarget(ctx);

  auto report = runFileOperation(
      ctx,
      [&](const ExternalFileRef& ref, FileEntry& e) -> QString {
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

    e.action = FileAction::Unchanged;
    e.size = QFileInfo{e.sourcePath}.size();
    return {};
      },
      /*dryRun=*/true);

  report.projectFolder = target.folder;
  return report;
}

void FileIndex::scan(const QString& folder, int maxFiles)
{
  m_root = folder;
  m_byName.clear();
  m_truncated = false;

  if(folder.isEmpty())
    return;

  QDirIterator it{
      folder, QDir::Files | QDir::NoDotAndDotDot | QDir::Readable,
      QDirIterator::Subdirectories};

  while(it.hasNext())
  {
    const QString path = it.next();
    if(m_byName.size() >= maxFiles)
    {
      m_truncated = true;
      break;
    }
    m_byName.insert(it.fileName().toLower(), path);
  }
}

std::vector<QString>
FileIndex::candidates(const QString& missingPath, qint64 size) const
{
  const QString name = QFileInfo{missingPath}.fileName().toLower();
  if(name.isEmpty())
    return {};

  std::vector<QString> found;
  for(auto it = m_byName.constFind(name); it != m_byName.constEnd() && it.key() == name;
      ++it)
    found.push_back(it.value());

  // Best first: a size match beats a name match, and among equals the file
  // closest to the folder the user pointed at is the likelier one.
  std::stable_sort(
      found.begin(), found.end(), [&](const QString& a, const QString& b) {
    if(size > 0)
    {
      const bool sa = QFileInfo{a}.size() == size;
      const bool sb = QFileInfo{b}.size() == size;
      if(sa != sb)
        return sa;
    }
    return a.count('/') < b.count('/');
      });

  return found;
}

FileReport
relinkFiles(const score::DocumentContext& ctx, const QHash<QString, QString>& chosen)
{
  const auto target = projectTarget(ctx);

  auto report = runFileOperation(
      ctx,
      [&](const ExternalFileRef& ref, FileEntry& e) -> QString {
    const auto it = chosen.constFind(ref.path);
    if(it == chosen.constEnd())
      return {};

    e.sourcePath = score::locateFilePath(ref.path, target.sourceRoots);
    e.destinationPath = *it;

    if(!QFileInfo::exists(e.destinationPath))
    {
      e.action = FileAction::Failed;
      e.note = QObject::tr("%1 does not exist").arg(e.destinationPath);
      return {};
    }

    // Store it relative when possible: a relink is a chance to make the
    // document more portable, not just to make it work once.
    e.newStoredPath
        = score::relativizeFilePath(e.destinationPath, target.destinationRoots);
    e.size = QFileInfo{e.destinationPath}.size();
    e.action = FileAction::Relinked;

    return e.newStoredPath;
      },
      /*dryRun=*/false, FileOperationKind::Relink);

  report.projectFolder = target.folder;
  return report;
}
}
