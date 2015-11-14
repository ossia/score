#pragma once
#include <Process/TimeValue.hpp>
#include <QPointF>
// A coordinate : (t, y)
namespace Scenario
{
struct Point
{
TimeValue date;
double y;
};

inline Point ConvertToScenarioPoint(
        const QPointF& point,
        ZoomRatio zoom,
        double height)
{
    return {TimeValue::fromMsecs(point.x() * zoom),
                point.y() / height};
}
}
