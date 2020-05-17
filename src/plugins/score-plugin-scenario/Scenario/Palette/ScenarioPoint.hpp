#pragma once
#include <Process/TimeValue.hpp>

#include <QPointF>

#include <verdigris>
// A coordinate : (t, y)
namespace Scenario
{
struct Point
{
  TimeVal date;
  double y;
};

inline Point ConvertToScenarioPoint(const QPointF& point, ZoomRatio zoom, double height) noexcept
{
  return {TimeVal::fromPixels(point.x(), zoom), point.y() / height};
}

inline QPointF ConvertFromScenarioPoint(const Point& point, ZoomRatio zoom, double height) noexcept
{
  return {point.date.toPixels(zoom), point.y * height};
}
}

Q_DECLARE_METATYPE(Scenario::Point)
W_REGISTER_ARGTYPE(Scenario::Point)
