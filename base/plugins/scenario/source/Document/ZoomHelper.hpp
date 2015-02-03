#pragma once

inline double secondsPerPixel(int zoomLevel)
{
	// sliderval = 20 => 1 pixel = 10ms
	// sliderval = 50 => 1 pixel = 1ms
	// sliderval = 80 => 1 pixel = 0.01ms

	// Formule : y = 46.73630963 e^(x * -7.707388206 * 10^-2 )
	//	double centralTime = (m_viewportStartTime + m_viewportEndTime) / 2.0; // Replace with cursor in the future
	return 46.73630963 * std::exp(-0.07707388206 * zoomLevel);
}
