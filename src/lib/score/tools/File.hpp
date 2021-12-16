#pragma once
#include <score/document/DocumentContext.hpp>

#include <QFileInfo>

#include <string_view>

namespace score
{

//! Will also look where the save file is located.
//! TODO in the future also look in a "common" library folder.
SCORE_LIB_BASE_EXPORT
QString locateFilePath(
    const QString& filename,
    const score::DocumentContext& ctx) noexcept;

inline QByteArray mapAsByteArray(QFile& f) noexcept
{
  const auto sz = f.size();
  if (auto data = f.map(0, sz))
  {
    return QByteArray::fromRawData(reinterpret_cast<const char*>(data), sz);
  }
  else
  {
    return {};
  }
}

inline std::string_view mapAsStringView(QFile& f) noexcept
{
  const auto sz = f.size();
  if (auto data = f.map(0, sz))
  {
    return std::string_view(reinterpret_cast<const char*>(data), sz);
  }
  else
  {
    return {};
  }
}

inline QString readFileAsQString(QFile& f) noexcept
{
  const auto sz = f.size();
  if (auto data = f.map(0, sz))
  {
    auto str = QString::fromUtf8(reinterpret_cast<const char*>(data), sz);
    f.unmap(data);
    return str;
  }
  else
  {
    return {};
  }
}

}
