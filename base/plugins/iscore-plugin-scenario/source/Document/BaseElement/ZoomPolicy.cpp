#include "ZoomPolicy.hpp"
#include <algorithm>

ZoomRatio ZoomPolicy::sliderPosToZoomRatio(double sliderPos, double cstrMaxTime, int cstrMaxWidth)
{
    // sliderPos is in [0;1] : 0 is zoom max, 1 zoom min.
    auto max = std::max(
                        24.,
                        20 + cstrMaxTime/cstrMaxWidth
                        );

    auto mapZoom = [] (double val, double min, double max)
    { return (max - min) * val + min; };

    return mapZoom(1 - sliderPos, zMin, max);
}

double ZoomPolicy::zoomRatioToSliderPos(ZoomRatio& z, double cstrMaxTime, int cstrMaxWidth)
{
    ZoomRatio zMax = cstrMaxTime/cstrMaxWidth;
    if(z == 0)
        z = zMax;

    return ((zMax - z) / (zMax - zMin));
}
