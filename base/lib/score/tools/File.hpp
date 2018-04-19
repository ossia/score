#pragma once
#include <QFileInfo>
#include <core/document/Document.hpp>
#include <score/document/DocumentContext.hpp>

namespace score
{

//! Will also look where the save file is located.
//! TODO in the future also look in a "common" library folder.
inline QString
locateFilePath(const QString& filename, const score::DocumentContext& ctx)
{
  QFileInfo info(filename);
  QString path = filename;

  if (!info.isAbsolute())
  {
    QFileInfo docroot{ctx.document.metadata().fileName()};
    path = docroot.canonicalPath();
    if (!path.endsWith('/'))
      path += '/';
    path += filename;
  }

  return path;
}
}
