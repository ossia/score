#include <score/tools/Uri.hpp>

#include <core/document/Document.hpp>

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace score
{
const QString& remoteUriMimeType() noexcept
{
  static const QString t = QStringLiteral("application/x-score-remote-uri");
  return t;
}

namespace
{
constexpr auto project_token = "<PROJECT>:";
constexpr auto library_token = "<LIBRARY>:";
constexpr auto cache_token = "<CACHE>:";

// Windows and macOS reach the same file through different spellings, so a
// comparison that respects case would fail to notice a file is inside the
// project folder and store an absolute path instead.
constexpr Qt::CaseSensitivity path_case()
{
#if defined(_WIN32) || defined(__APPLE__)
  return Qt::CaseInsensitive;
#else
  return Qt::CaseSensitive;
#endif
}

QString withoutTrailingSlash(QString dir)
{
  while(dir.size() > 1 && dir.endsWith('/'))
    dir.chop(1);
  return dir;
}

QString projectRoot(const DocumentContext& ctx)
{
  return QFileInfo{ctx.document.metadata().fileName()}.canonicalPath();
}

QString libraryRoot()
{
  QSettings set;
  const auto library = set.value("Library/RootPath").toString();
  if(library.isEmpty() || !QDir{library}.exists())
    return {};
  return QFileInfo{library}.canonicalFilePath();
}

QString join(const QString& dir, const QString& rest)
{
  if(dir.isEmpty())
    return {};
  return QFileInfo{withoutTrailingSlash(dir) + '/' + rest}.absoluteFilePath();
}

//! The part of `path` below `dir`, assuming isUnder(path, dir).
QString below(const QString& path, const QString& dir)
{
  QString rest = path.mid(withoutTrailingSlash(dir).size());
  while(rest.startsWith('/'))
    rest.remove(0, 1);
  return rest;
}
}

bool isUnder(const QString& path, const QString& dir) noexcept
{
  if(dir.isEmpty() || path.isEmpty())
    return false;

  const QString root = withoutTrailingSlash(dir);
  if(!path.startsWith(root, path_case()))
    return false;

  // Equal, or the next character starts a new component. Without this,
  // "/a/proj2/x" counts as being under "/a/proj".
  return path.size() == root.size() || path[root.size()] == '/';
}

QString mediaCacheRoot() noexcept
{
  const auto base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  if(base.isEmpty())
    return {};
  return base + "/media";
}

Uri Uri::parse(const QString& stored) noexcept
{
  if(stored.startsWith(project_token))
    return {UriScheme::Project, stored.mid(qstrlen(project_token))};
  if(stored.startsWith(library_token))
    return {UriScheme::Library, stored.mid(qstrlen(library_token))};
  if(stored.startsWith(cache_token))
    return {UriScheme::Cache, stored.mid(qstrlen(cache_token))};

  if(!stored.isEmpty() && !QFileInfo{stored}.isAbsolute())
    return {UriScheme::Relative, stored};

  return {UriScheme::Absolute, stored};
}

Uri Uri::relativize(const QString& absolute, const DocumentContext& ctx) noexcept
{
  const QFileInfo info{absolute};
  if(!info.isAbsolute())
    return parse(absolute);

  // Resolve symlinks so the comparison sees the same spelling the roots do.
  // A file that does not exist yet has no canonical path; use it as given.
  QString path = info.canonicalFilePath();
  if(path.isEmpty())
    path = absolute;

  if(const auto root = projectRoot(ctx); isUnder(path, root))
    return {UriScheme::Project, below(path, root)};

  if(const auto root = libraryRoot(); isUnder(path, root))
    return {UriScheme::Library, below(path, root)};

  if(const auto root = mediaCacheRoot(); isUnder(path, root))
    return {UriScheme::Cache, below(path, root)};

  return {UriScheme::Absolute, path};
}

QString Uri::toString() const noexcept
{
  switch(scheme)
  {
    case UriScheme::Project:
      return project_token + path;
    case UriScheme::Library:
      return library_token + path;
    case UriScheme::Cache:
      return cache_token + path;
    case UriScheme::Relative:
    case UriScheme::Absolute:
      break;
  }
  return path;
}

QString Uri::resolve(const DocumentContext& ctx) const noexcept
{
  switch(scheme)
  {
    case UriScheme::Project:
      return join(projectRoot(ctx), path);
    case UriScheme::Library:
      return join(libraryRoot(), path);
    case UriScheme::Cache:
      return join(mediaCacheRoot(), path);
    case UriScheme::Relative:
      return join(projectRoot(ctx), path);
    case UriScheme::Absolute:
      break;
  }
  return path.isEmpty() ? QString{} : QFileInfo{path}.absoluteFilePath();
}
}
