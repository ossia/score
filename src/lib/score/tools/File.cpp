#include <score/tools/File.hpp>
#include <core/document/Document.hpp>

namespace score
{

QString locateFilePath(
    const QString& filename,
    const score::DocumentContext& ctx) noexcept
{
  const QFileInfo info{filename};
  QString path = filename;

  if (!info.isAbsolute())
  {
    const QFileInfo docroot{ctx.document.metadata().fileName()};
    path = docroot.canonicalPath();
    if (!path.endsWith('/'))
      path += '/';
    path += filename;
  }

  return QFileInfo{path}.absoluteFilePath();
}

}
