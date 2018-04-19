#pragma once
#include <QDataStream>
#include <QDebug>
#include <QString>
#include <boost/functional/hash.hpp>

inline QDebug operator<<(QDebug debug, const std::string& obj)
{
  debug << obj.c_str();
  return debug;
}

namespace boost
{
template <>
struct hash<QString>
{
  std::size_t operator()(const QString& path) const
  {
    return qHash(path);
  }
};
}
