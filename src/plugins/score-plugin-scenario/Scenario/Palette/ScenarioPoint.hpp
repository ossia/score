#pragma once
#include <Process/TimeValue.hpp>

#include <QPointF>

#include <wobjectdefs.h>
// A coordinate : (t, y)
namespace Scenario
{
struct Point
{
  TimeVal date;
  double y;
};

inline Point
ConvertToScenarioPoint(const QPointF& point, ZoomRatio zoom, double height)
{
  return {TimeVal::fromMsecs(point.x() * zoom), point.y() / height};
}
}

Q_DECLARE_METATYPE(Scenario::Point)
W_REGISTER_ARGTYPE(Scenario::Point)
