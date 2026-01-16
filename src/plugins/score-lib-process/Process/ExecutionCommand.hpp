#pragma once

#include <cmath>
#include <smallfun.hpp>

#include <functional>
#include <version>
namespace Execution
{
using ExecutionCommand = smallfun::function<
    void(),
#if defined(_MSC_VER)
    256,
#else
    128,
#endif
    std::max((int)8, (int)std::max(alignof(std::function<void()>), alignof(double))),
    smallfun::Methods::Move>;

using GCCommand = smallfun::function<
    void(),
#if defined(_MSC_VER)
    2 * (128 + 4 * 8),
#else
    128 + 4 * 8,
#endif
    std::max((int)8, (int)std::max(alignof(std::function<void()>), alignof(double))),
    smallfun::Methods::Move>;
}
