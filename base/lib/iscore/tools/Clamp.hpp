#pragma once

inline double clamp(double val, double min, double max)
{
    return val < min ? min : (val > max ? max : val);
}