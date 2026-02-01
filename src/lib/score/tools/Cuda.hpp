#pragma once
#include <score/config.hpp>

#include <score_lib_base_export.h>

#include <utility>
namespace score
{
SCORE_LIB_BASE_EXPORT
std::pair<int, int> availableCudaDevice() noexcept;
SCORE_LIB_BASE_EXPORT
bool availableCudaToolkitDylibs(int major, int minor) noexcept;
}
