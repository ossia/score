#pragma once

using ZoomRatio = double;

inline ZoomRatio millisecondsPerPixel(int zoomLevel)
{
    return 100.0 / (zoomLevel > 10 ? zoomLevel : 10);
}
