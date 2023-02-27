#include <score/tools/File.hpp>

#include <core/document/Document.hpp>

#include <QDir>
#include <QSettings>

#include <iostream>
namespace score
{
// Taken from https://stackoverflow.com/a/18073392/1495627
// Adds a unique suffix to a file name so no existing file has the same file
// name. Can be used to avoid overwriting existing files. Works for both
// files/directories, and both relative/absolute paths. The suffix is in the
// form - "path/to/file.tar.gz", "path/to/file (1).tar.gz",
// "path/to/file (2).tar.gz", etc.
QString addUniqueSuffix(const QString& fileName)
{
  // If the file doesn't exist return the same name.
  if(!QFile::exists(fileName))
    return fileName;

  QFileInfo fileInfo{fileName};
  QString ret;

  // Split the file into 2 parts - dot+extension, and everything else. For
  // example, "path/file.tar.gz" becomes "path/file"+".tar.gz", while
  // "path/file" (note lack of extension) becomes "path/file"+"".
  QString secondPart = fileInfo.completeSuffix();
  QString firstPart;
  if(!secondPart.isEmpty())
  {
    secondPart = "." + secondPart;
    firstPart = fileName.left(fileName.size() - secondPart.size());
  }
  else
  {
    firstPart = fileName;
  }

  // Try with an ever-increasing number suffix, until we've reached a file
  // that does not yet exist.
  for(int i = 1;; i++)
  {
    // Construct the new file name by adding the unique number between the
    // first and second part.
    ret = QString("%1 (%2)%3").arg(firstPart).arg(i).arg(secondPart);
    // If no file exists with the new name, return it.
    if(!QFile::exists(ret))
    {
      return ret;
    }
  }
}

QString
locateFilePath(const QString& filename, const score::DocumentContext& ctx) noexcept
{
  const QFileInfo info{filename};
  QString path = filename;

  if(filename.startsWith("<PROJECT>:"))
  {
    const QFileInfo docroot{ctx.document.metadata().fileName()};
    path.replace("<PROJECT>:", docroot.canonicalPath() + "/");
  }
  else if(filename.startsWith("<LIBRARY>:"))
  {
    QSettings set;
    path.replace("<LIBRARY>:", set.value("Library/RootPath").toString() + "/");
  }
  else if(!info.isAbsolute())
  {
    const QFileInfo docroot{ctx.document.metadata().fileName()};
    path = docroot.canonicalPath();
    if(!path.endsWith('/'))
      path += '/';
    path += filename;
  }

  return QFileInfo{path}.absoluteFilePath();
}

QString
relativizeFilePath(const QString& filename, const score::DocumentContext& ctx) noexcept
{
  const QFileInfo info{filename};
  QString path = filename;

  if(info.isAbsolute())
  {
    const QFileInfo docroot{ctx.document.metadata().fileName()};
    const auto& docpath = docroot.canonicalPath();
    // 1. Check for whether the file is in the project's folder
    if(path.startsWith(docpath))
    {
      path.remove(0, docpath.length());
      while(path.startsWith('/'))
        path.remove(0, 1);

      path.prepend("<PROJECT>:");
    }
    else
    {
      // 2. Check whether it's in the user library
      QSettings set;
      if(auto library = set.value("Library/RootPath").toString(); QDir{library}.exists())
      {
        if(path.startsWith(library))
        {
          path.remove(0, library.length());
          if(path.startsWith('/'))
            path.remove(0, 1);

          path.prepend("<LIBRARY>:");
        }
      }
    }
  }

  return path;
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
  if(auto last_dot = completeBaseName.rfind('.'); last_dot != std::string_view::npos)
    completeBaseName.remove_suffix(completeBaseName.size() - last_dot);

  // Compute base name
  baseName = completeBaseName;
  if(auto first_dot = baseName.find('.'); first_dot != std::string_view::npos)
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
