#pragma once
#include <ProcessInterface/TimeValue.hpp>
#include <QPointF>
// A coordinate : (t, y)
struct ScenarioPoint
{
        TimeValue date;
        double y;
};

inline ScenarioPoint ConvertToScenarioPoint(
        const QPointF& point,
        ZoomRatio zoom,
        double height)
{
    return {TimeValue::fromMsecs(point.x() * zoom),
            point.y() / height};
}
