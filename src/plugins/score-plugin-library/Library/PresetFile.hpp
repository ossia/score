#pragma once
#include <QByteArray>
#include <QSaveFile>
#include <QString>
#include <QStringView>

namespace Library
{
//! The whole file or nothing: a failed write leaves what was there
[[nodiscard]] inline bool writePresetData(const QString& path, const QByteArray& data)
{
  QSaveFile f{path};
  if(!f.open(QIODevice::WriteOnly))
    return false;
  if(f.write(data) != data.size())
  {
    f.cancelWriting();
    return false;
  }
  return f.commit();
}

//! A name the library can give a file, on every platform
[[nodiscard]] inline bool isValidForFilename(QStringView name)
{
  if(name.trimmed().isEmpty() || name.startsWith(u'.'))
    return false;
  for(QChar c : QStringView{u",^@=+{}[]~!?:&*\"|#%<>$';`/\\"})
    if(name.contains(c))
      return false;
  return true;
}
}
