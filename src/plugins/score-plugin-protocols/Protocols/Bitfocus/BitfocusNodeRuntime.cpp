#include "BitfocusContext.hpp"

#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>

#include <ossia/detail/flat_map.hpp>

#include <QDir>
#include <QDirIterator>
#include <QFile>

namespace bitfocus
{
QString nodeExecutable(const QString& nodeVersion)
{
  static ossia::flat_map<QString, QString> node_path_cache;
  if(node_path_cache.empty())
  {
    const auto& set = score::AppContext().settings<Library::Settings::Model>();
    QString path = set.getPackagesPath() + "/companion-modules/node-runtime";
    if(QDir{path}.exists())
    {
      QDirIterator d{
          path, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags};
      while(d.hasNext())
      {
        QString name = d.next();
        auto version = QDir{name}.dirName().split('.');
        if(!version.isEmpty())
        {
          QString path = name;
#if defined(_WIN32)
          path += "/node.exe";
#else
          path += "/bin/node";
#endif
          node_path_cache["node" + version.front()] = path;

          QFile p{path};
          p.setPermissions(p.permissions() | QFile::Permission::ExeUser);
        }
      }
    }
  }

  if(auto it = node_path_cache.find(nodeVersion); it != node_path_cache.end())
    return it->second;

  // Hope it's in the PATH
#if defined(_WIN32)
  return "node.exe";
#else
  return "node";
#endif
}
}
