#include <algorithm>

#include <Process/ZoomHelper.hpp>
#include "ZoomPolicy.hpp"

ZoomRatio ZoomPolicy::sliderPosToZoomRatio(double sliderPos, double cstrMaxTime, int cstrMaxWidth)
{
    // sliderPos is in [0;1] : 0 is zoom max, 1 zoom min.
    auto zMax = std::max(
                        96.,
                        20 + cstrMaxTime/cstrMaxWidth
                        );

    auto mapZoom = [] (double val, double min, double max)
    { return (max - min) * val + min; };

    auto zMin = cstrMaxTime * 0.000001;
    return mapZoom(1 - sliderPos, zMin, zMax);
}

double ZoomPolicy::zoomRatioToSliderPos(ZoomRatio& z, double cstrMaxTime, int cstrMaxWidth)
{
    ZoomRatio zMax = cstrMaxTime/cstrMaxWidth + 20;
    if(z == 0)
        z = zMax;

    auto zMin = cstrMaxTime * 0.000001;
    return ((zMax - z) / (zMax - zMin));
}
