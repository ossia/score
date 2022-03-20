#pragma once
#include <score/model/path/Path.hpp>

#include <QDebug>

template <typename T>
QDebug& operator<<(QDebug& d, const Path<T>& path)
{
  auto& unsafe = path.unsafePath();
  d << unsafe.toString();
  return d;
}
