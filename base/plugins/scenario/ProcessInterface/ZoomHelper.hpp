#pragma once
#include <boost/algorithm/clamp.hpp>

// TODO use TimeValue to provide a mapping ?
inline double millisecondsPerPixel(int zoomLevel)
{
	if(zoomLevel < 10) zoomLevel = 10;
	return 100.0 / zoomLevel;
}
