#pragma once
#include "iscore_compiler_detection.hpp"

#if ISCORE_COMPILER_CXX_RELAXED_CONSTEXPR
#define ISCORE_RELAXED_CONSTEXPR constexpr
#else
#define ISCORE_RELAXED_CONSTEXPR
#endif
#ifdef ISCORE_DEBUG
template<typename T, typename U>
ISCORE_RELAXED_CONSTEXPR auto checked_cast(U&& other)
{
    return Q_ASSERT(dynamic_cast<T>(other)), dynamic_cast<T>(other);
}
#else
#define checked_cast static_cast
#endif
