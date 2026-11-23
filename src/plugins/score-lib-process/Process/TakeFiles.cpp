#include "TakeFiles.hpp"

#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>
#include <mutex>
#include <vector>

namespace Process
{
QString takesFolder(const score::DocumentContext& ctx)
{
  const QString file = ctx.document.metadata().fileName();
  if(QFileInfo fi{file}; fi.isAbsolute() && fi.exists())
    return fi.absolutePath() + "/Takes";

  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
         + "/recordings/" + QString::number(ctx.document.id().val());
}

int reserveTakeNumber(
    const QString& folder, const QString& prefix,
    const std::function<QString(int)>& fileFor)
{
  static std::mutex choosing;
  std::lock_guard _{choosing};

  QDir dir{folder};
  QDir{}.mkpath(folder);

  // A number is used when a file of the folder has it, whichever of a take's
  // files it is.
  const QString head = prefix + QStringLiteral("-take");
  std::vector<int> used;
  for(const QString& name :
      dir.entryList({head + QStringLiteral("*")}, QDir::Files | QDir::Hidden))
  {
    int end = int(head.size());
    while(end < name.size() && name[end].isDigit())
      end++;
    bool ok = false;
    const int n = name.mid(head.size(), end - head.size()).toInt(&ok);
    if(ok && end < name.size() && (name[end] == '.' || name[end] == '-'))
      used.push_back(n);
  }
  std::sort(used.begin(), used.end());
  int number = 1;
  for(int n : used)
    if(n == number)
      number++;
    else if(n > number)
      break;

  if(QFile f{fileFor(number)}; f.open(QIODevice::WriteOnly))
    f.close();
  return number;
}

QString
takeFile(const QString& folder, const QString& prefix, int number, const QString& extension)
{
  return QStringLiteral("%1/%2-take%3.%4").arg(folder, prefix).arg(number).arg(extension);
}
}
