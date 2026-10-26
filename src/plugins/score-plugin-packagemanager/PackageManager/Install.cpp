#include "Install.hpp"

#include <Library/SupportPackages.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

namespace PM
{
static bool removePath(const QString& path)
{
  const QFileInfo info{path};
  if(!info.exists() && !info.isSymLink())
    return true;

  if(info.isDir() && !info.isSymLink())
    return QDir{path}.removeRecursively();
  return QFile::remove(path);
}

static QString freeSiblingPath(const QString& path)
{
  QString candidate = path + ".old";
  for(int i = 1; QFileInfo::exists(candidate); i++)
    candidate = path + QStringLiteral(".old%1").arg(i);
  return candidate;
}

QString
archiveRootFolder(const QString& extractedIn, const std::vector<QString>& extracted)
{
  const QDir root{extractedIn};

  QString folder;
  for(const QString& path : extracted)
  {
    const QString relative = root.relativeFilePath(path);
    const int separator = relative.indexOf('/');
    if(separator <= 0)
      return {};

    const QString first = relative.left(separator);
    if(first == QStringLiteral(".."))
      return {};

    if(folder.isEmpty())
      folder = first;
    else if(folder != first)
      return {};
  }

  if(folder.isEmpty() || !QFileInfo{root.absoluteFilePath(folder)}.isDir())
    return {};

  return folder;
}

bool moveExtractedPackage(
    const QString& extractedIn, const std::vector<QString>& extracted,
    const QString& destination, QString& error)
{
  QDir extractedDir{extractedIn};
  const QString folder = archiveRootFolder(extractedIn, extracted);
  const QString source = folder.isEmpty() ? extractedDir.absolutePath()
                                          : extractedDir.absoluteFilePath(folder);

  if(!QFileInfo::exists(source))
  {
    error = QObject::tr("Nothing was extracted in %1").arg(extractedIn);
    return false;
  }

  QString previous;
  if(QFileInfo::exists(destination))
  {
    previous = freeSiblingPath(destination);
    if(!QDir{}.rename(destination, previous))
    {
      error = QObject::tr("Could not replace %1").arg(destination);
      return false;
    }
  }

  QDir{}.mkpath(QFileInfo{destination}.absolutePath());

  if(!QDir{}.rename(source, destination))
  {
    error = QObject::tr("Could not move %1 to %2").arg(source, destination);
    if(!previous.isEmpty())
      QDir{}.rename(previous, destination);
    return false;
  }

  // A previous install still in use (its libraries loaded, on Windows) is
  // removed at the next start instead of being left next to the new one.
  if(!previous.isEmpty() && !removePath(previous))
    Library::removePackageFolder(previous);
  if(!folder.isEmpty())
    extractedDir.removeRecursively();

  return true;
}

int removeOtherInstalls(const QUuid& key, const QString& keep, const QStringList& roots)
{
  const QString kept = QFileInfo{keep}.absoluteFilePath();
  int count = 0;
  for(const QString& root : roots)
  {
    for(const auto& pkg : Library::installedPackages(root))
    {
      // A folder linked there by hand is the developer's, whatever it holds.
      if(pkg.key != key || pkg.path == kept || Library::isMarkedForRemoval(pkg.path)
         || Library::isLinkedPackageFolder(pkg.path))
        continue;
      Library::removePackageFolder(pkg.path);
      count++;
    }
  }
  return count;
}
}
