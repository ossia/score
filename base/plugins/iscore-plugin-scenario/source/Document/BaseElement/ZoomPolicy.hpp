#pragma once
#include "ProcessInterface/ZoomHelper.hpp"

namespace ZoomPolicy
{
    ZoomRatio sliderPosToZoomRatio(double sliderPos, double cstrMaxTime, int cstrMaxWidth);
    double zoomRatioToSliderPos(ZoomRatio& z, double cstrMaxTime, int cstrMaxWidth);
    const ZoomRatio zMin = 16.; // experiment value
}

