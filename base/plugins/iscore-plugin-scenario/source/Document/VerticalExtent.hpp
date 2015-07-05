#pragma once

struct VerticalExtent
{
      double top{};
      double bottom{};
};

#include <QDebug>
inline QDebug operator<< (QDebug d, const VerticalExtent& ve)
{
    d << ve.top << ve.bottom;
    return d;
}
