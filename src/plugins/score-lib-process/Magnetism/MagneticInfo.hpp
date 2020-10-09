#pragma once
#include <Process/TimeValue.hpp>

#include <functional>
#include <score_lib_process_export.h>

namespace Process
{
struct SCORE_LIB_PROCESS_EXPORT MagneticInfo
{
  TimeVal time;
  bool snapLine{};
  operator const TimeVal&() const noexcept { return time; }
  operator TimeVal&() noexcept { return time; }
};

using MagnetismHandler = std::function<MagneticInfo(const QObject*, TimeVal)>;
}
