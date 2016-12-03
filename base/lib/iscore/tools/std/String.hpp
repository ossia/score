#pragma once
#include <QDataStream>
#include <QDebug>

inline QDebug operator<<(QDebug debug, const std::string& obj)
{
  debug << obj.c_str();
  return debug;
}
