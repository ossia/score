#pragma once
#include "ProcessInterface/ZoomHelper.hpp"

namespace ZoomPolicy
{
    ZoomRatio sliderPosToZoomRatio(double sliderPos, ZoomRatio min, double cstrMaxTime, int cstrMaxWidth);
}

