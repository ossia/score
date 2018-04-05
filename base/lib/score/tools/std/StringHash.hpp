#pragma once
#include <ossia-config.hpp>
#include <functional>
#include <QString>
#include <QHashFunctions>

// TODO merge String.hpp here
namespace std
{
template<>
struct hash<QString>
{
  std::size_t operator()(const QString& path) const
  {
    return qHash(path);
  }
};
}
