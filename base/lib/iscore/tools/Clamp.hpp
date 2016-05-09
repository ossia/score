#pragma once

template<typename T, typename U, typename V>
inline auto clamp(T val, U min, V max)
{
    return val < min ? min : (val > max ? max : val);
}
