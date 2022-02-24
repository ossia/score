#include <score/tools/File.hpp>
#include <core/document/Document.hpp>
#include <iostream>
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

PathInfo::PathInfo(std::string_view v) noexcept
  : absoluteFilePath{v}
{
  // Find the last dir separator
  auto last_slash = absoluteFilePath.rfind('/');
  if(last_slash == std::string_view::npos)
    last_slash = absoluteFilePath.rfind('\\');

  // Compute file name
  fileName = absoluteFilePath;
  if(last_slash != std::string_view::npos)
    fileName.remove_prefix(last_slash + 1);

  // Compute complete base name
  completeBaseName = fileName;
  if(auto last_dot = completeBaseName.rfind('.');
     last_dot != std::string_view::npos)
    completeBaseName.remove_suffix(completeBaseName.size() - last_dot);

  // Compute base name
  baseName = completeBaseName;
  if(auto first_dot = baseName.find('.');
     first_dot != std::string_view::npos)
    baseName.remove_prefix(first_dot + 1);

  // Compute parent dir
  absolutePath = absoluteFilePath;
  if(last_slash != std::string_view::npos)
    absolutePath.remove_suffix(absolutePath.size() - last_slash);

  // Compute parent dir name
  parentDirName = absolutePath;
  last_slash = absolutePath.rfind('/');
  if(last_slash == std::string_view::npos)
    last_slash = absolutePath.rfind('\\');
  if(last_slash != std::string_view::npos)
    parentDirName.remove_prefix(last_slash + 1);
/*
  std::cerr << "input: " << absoluteFilePath << "\n";
  std::cerr << " - filename: " << fileName << "\n";
  std::cerr << " - completeBaseName: " << completeBaseName << "\n";
  std::cerr << " - baseName: " << baseName << "\n";
  std::cerr << " - absolutePath: " << absolutePath << "\n";
  std::cerr << " - parentDirName: " << parentDirName << "\n\n";
*/
}
}
