#pragma once
#include <score/document/DocumentContext.hpp>
#include <score/tools/FilePath.hpp>

#include <QFileInfo>

#include <string_view>

namespace score
{

// Used instead of QFileInfo
// as it does a stat which can be super expensive
// when scanning large libraries ; this class only extracts
// path info from the string.
// Note: it works on string_view, that is, it should only
// be used for transient computations as it won't allocate memory for the
// path string it is created with.
struct SCORE_LIB_BASE_EXPORT PathInfo
{
public:
  explicit PathInfo(std::string_view v) noexcept;

  // Absolute path to the file, passed as input.
  // ex. /home/user/foo.tar.gz
  const std::string_view absoluteFilePath;

  // foo.tar.gz
  std::string_view fileName;

  // foo.tar
  std::string_view completeBaseName;

  // foo
  std::string_view baseName;

  // /home/user
  std::string_view absolutePath;

  // user
  std::string_view parentDirName;
};

inline QByteArray mapAsByteArray(QFile& f) noexcept
{
  const auto sz = f.size();
  if(auto data = f.map(0, sz))
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
  if(auto data = f.map(0, sz))
  {
    return std::string_view(reinterpret_cast<const char*>(data), sz);
  }
  else
  {
    return {};
  }
}

inline std::string readFileAsString(QFile& f) noexcept
{
  std::string str;
  const auto sz = f.size();
  if(auto data = f.map(0, sz))
  {
    str = std::string(reinterpret_cast<const char*>(data), sz);
    f.unmap(data);
  }
  return str;
}

inline QString readFileAsQString(QFile& f) noexcept
{
  QString str;
  const auto sz = f.size();
  if(auto data = f.map(0, sz))
  {
    str = QString::fromUtf8(reinterpret_cast<const char*>(data), sz);
    f.unmap(data);
  }
  return str;
}

}
