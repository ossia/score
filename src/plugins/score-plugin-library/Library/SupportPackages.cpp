#include <Library/SupportPackages.hpp>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <map>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace Library
{
// Left in a package folder which could not be removed.
static constexpr auto removal_marker = ".score-remove";

// The support packages loaded in this process: their keys, and the folders.
static QSet<QUuid> g_loadedKeys;
static QSet<QString> g_loadedFolders;

static QFileInfoList packageFolders(const QString& root)
{
  // ".incoming-<name>" folders are installs in progress, or interrupted. Not
  // QDir::Hidden: a leading dot is not what makes a file hidden on Windows.
  QFileInfoList folders
      = QDir{root}.entryInfoList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name);
  folders.removeIf([](const QFileInfo& f) { return f.fileName().startsWith('.'); });
  return folders;
}

static bool isLink(const QFileInfo& info)
{
#if defined(Q_OS_WIN)
  return info.isSymLink() || info.isJunction();
#else
  return info.isSymLink();
#endif
}

std::vector<InstalledPackage> installedPackages(const QString& root)
{
  std::vector<InstalledPackage> packages;
  for(const QFileInfo& folder : packageFolders(root))
  {
    QFile f{folder.absoluteFilePath() + "/package.json"};
    if(!f.open(QIODevice::ReadOnly))
      continue;

    const auto obj = QJsonDocument::fromJson(f.readAll()).object();
    // The same reading as the package manager's: a version is an integer.
    InstalledPackage pkg{
        folder.absoluteFilePath(), QUuid::fromString(obj["key"].toString()),
        obj["version"].toInt()};
    if(!pkg.key.isNull())
      packages.push_back(std::move(pkg));
  }
  return packages;
}

bool removePackageFolder(const QString& path)
{
  // A folder linked there by hand (a package under development): the link goes,
  // what it points to is not ours.
  if(const QFileInfo info{path}; isLink(info))
    return QFile::remove(path) || QDir{}.rmdir(path);

  QDir dir{path};
  if(!dir.exists() || dir.removeRecursively())
    return true;

  // Whatever could not be removed stays, along with the marker saying it is not
  // a package any more.
  if(QFile marker{dir.absoluteFilePath(removal_marker)}; marker.open(QIODevice::WriteOnly))
    qDebug() << "Package folder in use, it will be removed at the next start:" << path;
  else
    qDebug() << "Could not remove the package folder" << path;
  return false;
}

bool isMarkedForRemoval(const QString& path)
{
  return QFileInfo::exists(path + '/' + removal_marker);
}

bool isLinkedPackageFolder(const QString& path)
{
  return isLink(QFileInfo{path});
}

void removeMarkedPackageFolders(const QString& root)
{
  for(const QFileInfo& folder : packageFolders(root))
  {
    if(!isLink(folder) && isMarkedForRemoval(folder.absoluteFilePath()))
    {
      if(!QDir{folder.absoluteFilePath()}.removeRecursively())
        qDebug() << "Could not remove the package folder" << folder.absoluteFilePath();
    }
  }
}

void removeInterruptedInstalls(const QString& root, std::chrono::seconds age)
{
  const QDateTime before = QDateTime::currentDateTimeUtc().addSecs(-age.count());
  const auto entries = QDir{root}.entryInfoList(
      {".incoming-*", ".zdl-download-*"},
      QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot | QDir::System);
  for(const QFileInfo& entry : entries)
  {
    // Another score running may be installing there right now; an install
    // that has not been written to for that long is not going anywhere.
    if(entry.lastModified().toUTC() > before)
      continue;
    const bool removed = (entry.isDir() && !isLink(entry))
                             ? QDir{entry.absoluteFilePath()}.removeRecursively()
                             : QFile::remove(entry.absoluteFilePath());
    if(!removed)
      qDebug() << "Could not remove the interrupted install" << entry.absoluteFilePath();
  }
}

QStringList supportFoldersToLoad(const QString& supportDir)
{
  // Keyed by the package key: the newest version seen so far.
  std::map<QUuid, InstalledPackage> newest;
  for(auto& pkg : installedPackages(supportDir))
  {
    if(isMarkedForRemoval(pkg.path))
      continue;

    auto [it, inserted] = newest.try_emplace(pkg.key, pkg);
    if(!inserted)
    {
      const bool newer = pkg.version > it->second.version;
      qDebug() << "Support package installed twice, not loading"
               << (newer ? it->second.path : pkg.path);
      if(newer)
        it->second = std::move(pkg);
    }
  }

  QSet<QString> keep;
  for(auto& [key, pkg] : newest)
    keep.insert(pkg.path);

  QStringList folders;
  for(const QFileInfo& folder : packageFolders(supportDir))
  {
    const QString path = folder.absoluteFilePath();
    if(isMarkedForRemoval(path))
      continue;
    // Folders without a package.json were put there by hand: always loaded.
    if(keep.contains(path) || !QFileInfo::exists(path + "/package.json"))
      folders.push_back(path);
  }
  return folders;
}

static QUuid packageKey(const QString& folder)
{
  QFile f{folder + "/package.json"};
  if(!f.open(QIODevice::ReadOnly))
    return {};
  return QUuid::fromString(QJsonDocument::fromJson(f.readAll()).object()["key"].toString());
}

QStringList loadSupportPackages(const QString& supportDir)
{
  QStringList folders;
  for(const QString& folder : supportFoldersToLoad(supportDir))
  {
    // Loaded already: loading again retries what failed then, typically for
    // want of a package installed since. Another version of a loaded package
    // waits for a restart.
    const QUuid key = packageKey(folder);
    if(g_loadedFolders.contains(folder) || key.isNull() || !g_loadedKeys.contains(key))
      folders.push_back(folder);
  }

  for(const QString& folder : folders)
  {
    g_loadedFolders.insert(folder);
    if(const QUuid key = packageKey(folder); !key.isNull())
      g_loadedKeys.insert(key);
  }
  loadSupportLibraries(folders);
  return folders;
}

bool isSupportPackageLoaded(const QUuid& key)
{
  return g_loadedKeys.contains(key);
}

QStringList supportLibraryFiles(const QStringList& folders, const QString& bundledDir)
{
#if defined(Q_OS_MACOS)
  const QStringList patterns{"*.dylib"};
#else
  const QStringList patterns{"*.so", "*.so.*"};
#endif
  const QDir bundled{bundledDir};

  // Symbolic links are not loaded: packages ship libraries the way their vendors
  // lay them out, a real file (libcudnn_graph.so.9.27.0) and links to it under
  // the names it is looked up by (libcudnn_graph.so.9). The links are what makes
  // the libraries find each other through their $ORIGIN runpath instead of a
  // system copy; loading the file once is enough.
  //
  // What the application bundles wins: a custom app (koaia) ships CUDA, cuDNN
  // and TensorRT in its lib folder under their sonames, so a package's file is
  // skipped when any of its names, its links' included, is bundled.
  QStringList candidates;
  QSet<QString> bundledFiles;
  for(const QString& folder : folders)
  {
    for(QDirIterator it(folder, patterns, QDir::Files, QDirIterator::Subdirectories);
        it.hasNext();)
    {
      const QString path = it.next();
      const bool isBundled = bundled.exists(it.fileName());
      if(it.fileInfo().isSymLink())
      {
        if(isBundled)
          bundledFiles.insert(it.fileInfo().canonicalFilePath());
      }
      else if(!isBundled)
      {
        candidates.push_back(path);
      }
    }
  }

  QStringList files;
  for(const QString& path : candidates)
    if(!bundledFiles.contains(QFileInfo{path}.canonicalFilePath()))
      files.push_back(path);
  return files;
}

void loadSupportLibraries(const QStringList& folders)
{
  if(folders.isEmpty())
    return;
#ifdef Q_OS_WIN
  // On Windows we can extend the DLL search path: register every directory that
  // contains a .dll, so a later LoadLibrary("foo.dll") from an addon resolves.
  // AddDllDirectory only extends the search set used by loads that pass
  // LOAD_LIBRARY_SEARCH_USER_DIRS; a bare LoadLibrary (what the addons do) uses the
  // legacy order and ignores it. This makes those directories part of the process-wide
  // default instead.
  QSet<QString> added;

  for(const QString& folder : folders)
  {
    QDirIterator it(folder, {"*.dll"}, QDir::Files, QDirIterator::Subdirectories);

    while(it.hasNext())
    {
      it.next();
      QString dir = it.fileInfo().absolutePath();

      if(added.contains(dir))
        continue;
      // Not before there is a folder to add: it takes PATH out of the search for
      // every library the process loads afterwards, addons' dependencies included.
      if(added.isEmpty())
        SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
      added.insert(dir);

      AddDllDirectory(reinterpret_cast<PCWSTR>(dir.utf16()));
    }
  }
#else
  // On Linux/macOS there is no runtime equivalent of AddDllDirectory: the dynamic
  // loader snapshots LD_LIBRARY_PATH / DYLD_* at process start, so changing them
  // here would not affect later dlopen() calls. Instead, eagerly dlopen every
  // support library with RTLD_GLOBAL: a subsequent bare-name dlopen() from an addon
  // then resolves to the already-loaded object (the loader matches it by soname /
  // install name). We iterate to a fixpoint so that inter-dependent support
  // libraries load regardless of their on-disk order (each successful load makes its
  // symbols / NEEDED entries available to the others). Libraries that never load
  // (e.g. a missing external dependency) are simply skipped — the addon relying on
  // them just stays unavailable, exactly as before.
#if defined(Q_OS_MACOS)
  const QString bundled = QCoreApplication::applicationDirPath() + "/../Frameworks";
#else
  const QString bundled = QCoreApplication::applicationDirPath() + "/../lib";
#endif
  QStringList pending = supportLibraryFiles(folders, bundled);

  bool progress = true;
  while(progress && !pending.empty())
  {
    progress = false;
    for(auto it = pending.begin(); it != pending.end();)
    {
      if(dlopen(it->toUtf8().constData(), RTLD_LAZY | RTLD_GLOBAL))
      {
        it = pending.erase(it);
        progress = true;
      }
      else
      {
        ++it;
      }
    }
  }
#endif
}
}
