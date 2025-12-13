#include <score/tools/std/String.hpp>

#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
QDebug operator<<(QDebug debug, const std::string& obj)
{
  debug << obj.c_str();
  return debug;
}
QDebug operator<<(QDebug debug, std::string_view obj)
{
  debug << QByteArray::fromRawData(obj.data(), obj.size());
  return debug;
}
#endif
