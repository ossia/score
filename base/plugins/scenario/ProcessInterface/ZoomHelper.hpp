#pragma once

// TODO use TimeValue to provide a mapping ?
inline double secondsPerPixel(int zoomLevel)
{
	return 100.0 / zoomLevel;
}
