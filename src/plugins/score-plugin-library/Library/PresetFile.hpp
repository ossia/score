#pragma once
#include <QByteArray>
#include <QSaveFile>
#include <QString>

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
}
