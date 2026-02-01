#pragma once
#include <score/config.hpp>

#include <score_lib_base_export.h>

namespace score
{
SCORE_LIB_BASE_EXPORT
bool checkCudaSupported() noexcept;
}
