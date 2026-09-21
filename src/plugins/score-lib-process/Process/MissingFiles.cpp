#include <Process/MissingFiles.hpp>
#include <Process/ProjectConsolidation.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/tools/RecursiveWatch.hpp>
#include <score/tools/ThreadPool.hpp>

#include <QCoreApplication>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>

#include <algorithm>
#include <string>

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

void FileIndex::scan(const QString& folder, FileScan* progress)
{
  m_root = folder;
  m_byName.clear();

  if(folder.isEmpty())
    return;

  // Kept alive for the whole walk: the traversal wants a zero-terminated root.
  const std::string root = folder.toStdString();

  QString lastFolder;
  score::for_all_files(root, [&](std::string_view path) {
    if(progress && progress->cancelled())
      return;
    if(path.empty())
      return;

    const QString p = QString::fromUtf8(path.data(), path.size());
    qsizetype sep = p.lastIndexOf('/');
#if defined(_WIN32)
    sep = std::max(sep, p.lastIndexOf('\\'));
#endif
    if(sep < 0 || sep == p.size() - 1)
      return;

    m_byName.insert(p.sliced(sep + 1).toLower(), p);

    if(progress)
    {
      progress->fileSeen();

      // Publish on folder change, not per file.
      if(QStringView{p}.first(sep) != lastFolder)
      {
        lastFolder = p.first(sep);
        progress->setCurrentFolder(lastFolder);
      }
    }
  });
}

FileScan::FileScan(QString folder)
    : m_root{std::move(folder)}
{
}

QString FileScan::currentFolder() const
{
  std::lock_guard l{m_mutex};
  return m_currentFolder;
}

void FileScan::setCurrentFolder(const QString& folder)
{
  std::lock_guard l{m_mutex};
  m_currentFolder = folder;
}

std::shared_ptr<FileScan>
FileScan::start(const QString& folder, QObject* context, OnFinished onFinished)
{
  auto self = std::make_shared<FileScan>(folder);

  score::TaskPool::instance().post(
      [self, ctx = QPointer<QObject>{context}, cb = std::move(onFinished)]() mutable {
    FileIndex index;
    index.scan(self->root(), self.get());

    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [ctx, cb = std::move(cb), index = std::move(index),
         cancelled = self->cancelled()]() mutable {
      if(!ctx)
        return;
      cb(std::move(index), cancelled);
    },
        Qt::QueuedConnection);
      });

  return self;
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
