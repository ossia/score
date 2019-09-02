#pragma once
#include <QDebug>
#include <score/model/Identifier.hpp>

template <typename tag>
QDebug operator<<(QDebug dbg, const Id<tag>& c)
{
  dbg.nospace() << c.val();

  return dbg.space();
}
