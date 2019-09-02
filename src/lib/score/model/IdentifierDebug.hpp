#pragma once
#include <score/model/Identifier.hpp>

#include <QDebug>

template <typename tag>
QDebug operator<<(QDebug dbg, const Id<tag>& c)
{
  dbg.nospace() << c.val();

  return dbg.space();
}
