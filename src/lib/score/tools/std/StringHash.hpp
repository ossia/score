#pragma once
#include <QString>
#include <QHashFunctions>
#include <functional>

#include <ossia-config.hpp>

// TODO merge String.hpp here
namespace std
{
#if (QT_VERSION < QT_VERSION_CHECK(5, 14, 0))
template <>
struct hash<QString>
{
  std::size_t operator()(const QString& path) const { return qHash(path); }
};
#endif
}
