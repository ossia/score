#pragma once
#include <score/tools/FilePath.hpp>

#include <QByteArray>
#include <QDateTime>

#include <string>

namespace avnd_tools
{
static QByteArray
filter_filename(const std::string& f, const score::DocumentContext& ctx)
{
  auto filename = QString::fromStdString(f);
  filename = filename.trimmed();
  if(filename.startsWith('"') && filename.endsWith('"') && filename.length() > 2)
    filename = filename.mid(1, filename.length() - 2);
  auto t = QDateTime::currentDateTimeUtc().toString();
  t.replace(':', '_');
  filename.replace("%t", t);
  filename = score::locateFilePath(filename, ctx);
  return filename.toUtf8();
}
}
