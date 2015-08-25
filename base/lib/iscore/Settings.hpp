#pragma once
#include "iscore_compiler_detection.hpp"
#include <iscore/tools/Todo.hpp>

#if ISCORE_COMPILER_CXX_RELAXED_CONSTEXPR
#define ISCORE_RELAXED_CONSTEXPR constexpr
#else
#define ISCORE_RELAXED_CONSTEXPR
#endif


#ifdef ISCORE_DEBUG
template<typename T, typename U>
T checked_cast(U&& other)
{
    auto res = dynamic_cast<T>(other);
    ISCORE_ASSERT(res);
    return res;
}
#else
#define checked_cast static_cast
#endif
