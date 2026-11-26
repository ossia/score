#include "KeepTakes.hpp"

#include <Process/ExternalFiles.hpp>
#include <Process/TakeFiles.hpp>

#include <Recording/Commands/PlaceTakes.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/tools/FilePath.hpp>
#include <score/tools/ProjectFiles.hpp>

#include <core/document/Document.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <map>

namespace Recording
{
int keepTakesWithProject(score::Document& doc, const QString& newFileName)
{
  const auto& ctx = doc.context();
  // Saved before: its takes are already next to it
  if(QFileInfo fi{doc.metadata().fileName()}; fi.isAbsolute() && fi.exists())
    return 0;
  const QString from = Process::takesFolder(ctx);
  const QDir source{from};
  if(!source.exists())
    return 0;
  const auto files = source.entryInfoList(QDir::Files | QDir::Hidden);
  if(files.isEmpty())
    return 0;

  const QString to = Process::takesFolderIn(QFileInfo{newFileName}.absolutePath());
  if(!QDir{}.mkpath(to))
    return 0;

  // Every file, referenced or not: a take kept as a file only comes along
  std::map<QString, QString> moved;
  for(const auto& f : files)
  {
    QString dest = to + "/" + f.fileName();
    if(QFileInfo::exists(dest))
      dest = score::addUniqueSuffix(dest);
    if(QFile::copy(f.absoluteFilePath(), dest))
      moved[QFileInfo{f.absoluteFilePath()}.canonicalFilePath()] = dest;
  }
  if(moved.empty())
    return 0;

  const auto old_roots = score::pathRoots(ctx);
  Process::ExternalFileMap map{[&](const Process::ExternalFileRef& ref) -> QString {
    const QString abs = score::locateFilePath(ref.path, old_roots);
    auto it = moved.find(QFileInfo{abs}.canonicalFilePath());
    if(it == moved.end())
      return {};
    // Absolute: a project path would be resolved against where the document
    // is now, which is nowhere yet
    return QFileInfo{it->second}.absoluteFilePath();
  }};
  Process::mapDocumentExternalFiles(ctx, map);
  if(map.hasCommands())
  {
    MacroCommandDispatcher<KeepTakes> disp{ctx.commandStack};
    for(auto* cmd : map.takeCommands())
      disp.submit(cmd);
    disp.commit();
  }
  return int(moved.size());
}
}
